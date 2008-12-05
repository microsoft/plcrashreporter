//
//  GTMHTTPFetcher.m
//
//  Copyright 2007-2008 Google Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not
//  use this file except in compliance with the License.  You may obtain a copy
//  of the License at
// 
//  http://www.apache.org/licenses/LICENSE-2.0
// 
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
//  License for the specific language governing permissions and limitations under
//  the License.
//

#define GTMHTTPFETCHER_DEFINE_GLOBALS 1

#import "GTMHTTPFetcher.h"
#import "GTMDebugSelectorValidation.h"

@interface GTMHTTPFetcher (GTMHTTPFetcherLoggingInternal)
- (void)logFetchWithError:(NSError *)error;
- (void)logCapturePostStream;
@end

// Make sure that if logging is disabled, the InputStream logging is also
// diabled.
#if !GTM_HTTPFETCHER_ENABLE_LOGGING
# undef  GTM_HTTPFETCHER_ENABLE_INPUTSTREAM_LOGGING
# define GTM_HTTPFETCHER_ENABLE_INPUTSTREAM_LOGGING 0
#endif // GTM_HTTPFETCHER_ENABLE_LOGGING

#if GTM_HTTPFETCHER_ENABLE_INPUTSTREAM_LOGGING
#import "GTMProgressMonitorInputStream.h"
@interface GTMInputStreamLogger : GTMProgressMonitorInputStream
// GTMInputStreamLogger wraps any NSInputStream used for uploading so we can
// capture a copy of the data for the log
@end
#endif // !GTM_HTTPFETCHER_ENABLE_INPUTSTREAM_LOGGING

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
@interface NSURLConnection (LeopardMethodsOnTigerBuilds)
- (id)initWithRequest:(NSURLRequest *)request delegate:(id)delegate startImmediately:(BOOL)startImmediately;
- (void)start;
- (void)scheduleInRunLoop:(NSRunLoop *)aRunLoop forMode:(NSString *)mode;
@end
#endif

NSString* const kGTMLastModifiedHeader = @"Last-Modified";
NSString* const kGTMIfModifiedSinceHeader = @"If-Modified-Since";


NSMutableArray* gGTMFetcherStaticCookies = nil;
Class gGTMFetcherConnectionClass = nil;
NSArray *gGTMFetcherDefaultRunLoopModes = nil;

const NSTimeInterval kDefaultMaxRetryInterval = 60. * 10.; // 10 minutes
                   
@interface GTMHTTPFetcher (PrivateMethods)
- (void)setCookies:(NSArray *)newCookies
           inArray:(NSMutableArray *)cookieStorageArray;
- (NSArray *)cookiesForURL:(NSURL *)theURL inArray:(NSMutableArray *)cookieStorageArray;
- (void)handleCookiesForResponse:(NSURLResponse *)response;
- (BOOL)shouldRetryNowForStatus:(NSInteger)status error:(NSError *)error;
- (void)retryTimerFired:(NSTimer *)timer;
- (void)destroyRetryTimer;
- (void)beginRetryTimer;
- (void)primeTimerWithNewTimeInterval:(NSTimeInterval)secs;
- (void)retryFetch;
@end

@implementation GTMHTTPFetcher

+ (GTMHTTPFetcher *)httpFetcherWithRequest:(NSURLRequest *)request {
  return [[[GTMHTTPFetcher alloc] initWithRequest:request] autorelease];
}

+ (void)initialize {
  if (!gGTMFetcherStaticCookies) {
    gGTMFetcherStaticCookies = [[NSMutableArray alloc] init];
  }
}

- (id)init {
  return [self initWithRequest:nil];
}

- (id)initWithRequest:(NSURLRequest *)request {
  if ((self = [super init]) != nil) {

    request_ = [request mutableCopy];
    
    [self setCookieStorageMethod:kGTMHTTPFetcherCookieStorageMethodStatic];        
  }
  return self;
}

// TODO: do we need finalize to call stopFetching?

- (void)dealloc {
  [self stopFetching]; // releases connection_

  [request_ release];
  [downloadedData_ release];
  [credential_ release];
  [proxyCredential_ release];
  [postData_ release];
  [postStream_ release];
  [loggedStreamData_ release];
  [response_ release];
  [userData_ release];
  [runLoopModes_ release];
  [fetchHistory_ release];
  [self destroyRetryTimer];
  
  [super dealloc];
}

#pragma mark -

// Begin fetching the URL.  |delegate| is not retained
// The delegate must provide and implement the finished and failed selectors.
//
// finishedSEL has a signature like:
//   - (void)fetcher:(GTMHTTPFetcher *)fetcher finishedWithData:(NSData *)data
// failedSEL has a signature like:
//   - (void)fetcher:(GTMHTTPFetcher *)fetcher failedWithError:(NSError *)error

- (BOOL)beginFetchWithDelegate:(id)delegate
             didFinishSelector:(SEL)finishedSEL
               didFailSelector:(SEL)failedSEL {
  
  GTMAssertSelectorNilOrImplementedWithArguments(delegate, finishedSEL, @encode(GTMHTTPFetcher *), @encode(NSData *), NULL);
  GTMAssertSelectorNilOrImplementedWithArguments(delegate, failedSEL, @encode(GTMHTTPFetcher *), @encode(NSError *), NULL);
  GTMAssertSelectorNilOrImplementedWithArguments(delegate, receivedDataSEL_, @encode(GTMHTTPFetcher *), @encode(NSData *), NULL);
  GTMAssertSelectorNilOrImplementedWithReturnTypeAndArguments(delegate, retrySEL_, @encode(BOOL), @encode(GTMHTTPFetcher *), @encode(BOOL), @encode(NSError *), NULL);
    
  if (connection_ != nil) {
    _GTMDevAssert(connection_ != nil,
                  @"fetch object %@ being reused; this should never happen",
                  self);
    goto CannotBeginFetch;
  }
  
  if (request_ == nil) {
    _GTMDevAssert(request_ != nil, @"beginFetchWithDelegate requires a request");
    goto CannotBeginFetch;  
  }
  
  [downloadedData_ release];
  downloadedData_ = nil;
  
  [self setDelegate:delegate];
  finishedSEL_ = finishedSEL;
  failedSEL_ = failedSEL;
  
  if (postData_ || postStream_) {
    if ([request_ HTTPMethod] == nil || [[request_ HTTPMethod] isEqual:@"GET"]) {
      [request_ setHTTPMethod:@"POST"];
    }
    
    if (postData_) {
      [request_ setHTTPBody:postData_];
    } else {
      
      // if logging is enabled, it needs a buffer to accumulate data from any
      // NSInputStream used for uploading.  Logging will wrap the input
      // stream with a stream that lets us keep a copy the data being read.
      if ([GTMHTTPFetcher isLoggingEnabled] && postStream_ != nil) {
        loggedStreamData_ = [[NSMutableData alloc] init];
        [self logCapturePostStream];
      }
      
      [request_ setHTTPBodyStream:postStream_]; 
    }
  }
  
  if (fetchHistory_) {
    
    // If this URL is in the history, set the Last-Modified header field
    
    // if we have a history, we're tracking across fetches, so we don't
    // want to pull results from a cache
    [request_ setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    
    NSDictionary* lastModifiedDict = [fetchHistory_ objectForKey:kGTMHTTPFetcherHistoryLastModifiedKey];
    NSString* urlString = [[request_ URL] absoluteString];
    NSString* lastModifiedStr = [lastModifiedDict objectForKey:urlString];
    
    // servers don't want last-modified-ifs on POSTs, so check for a body
    if (lastModifiedStr 
        && [request_ HTTPBody] == nil
        && [request_ HTTPBodyStream] == nil) {
      
      [request_ addValue:lastModifiedStr forHTTPHeaderField:kGTMIfModifiedSinceHeader];
    }
  }
  
  // get cookies for this URL from our storage array, if
  // we have a storage array
  if (cookieStorageMethod_ != kGTMHTTPFetcherCookieStorageMethodSystemDefault) {
    
    NSArray *cookies = [self cookiesForURL:[request_ URL]];
    
    if ([cookies count]) {
      
      NSDictionary *headerFields = [NSHTTPCookie requestHeaderFieldsWithCookies:cookies];
      NSString *cookieHeader = [headerFields objectForKey:@"Cookie"]; // key used in header dictionary
      if (cookieHeader) {
        [request_ addValue:cookieHeader forHTTPHeaderField:@"Cookie"]; // header name
      }
    }
  }
  
  // finally, start the connection
	
  Class connectionClass = [[self class] connectionClass];
	
  NSArray *runLoopModes = nil;
  
  if ([[self class] doesSupportRunLoopModes]) {
    
    // use the connection-specific run loop modes, if they were provided,
    // or else use the GTMHTTPFetcher default run loop modes, if any
    if (runLoopModes_) {
      runLoopModes = runLoopModes_;
    } else  {
      runLoopModes = gGTMFetcherDefaultRunLoopModes;
    }
  }
  
  if ([runLoopModes count] == 0) {
    
    // if no run loop modes were specified, then we'll start the connection 
    // on the current run loop in the current mode
   connection_ = [[connectionClass connectionWithRequest:request_
                                                 delegate:self] retain];
  } else {
    
    // schedule on current run loop in the specified modes
    connection_ = [[connectionClass alloc] initWithRequest:request_
                                                  delegate:self 
                                          startImmediately:NO];
    
    for (NSUInteger idx = 0; idx < [runLoopModes count]; idx++) {
      NSString *mode = [runLoopModes objectAtIndex:idx];
      [connection_ scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:mode];
    }
    [connection_ start];
  }
  
  if (!connection_) {
    _GTMDevAssert(connection_ != nil,
                  @"beginFetchWithDelegate could not create a connection");
    goto CannotBeginFetch;
  }

  // we'll retain the delegate only during the outstanding connection (similar
  // to what Cocoa does with performSelectorOnMainThread:) since we'd crash
  // if the delegate was released in the interim.  We don't retain the selector
  // at other times, to avoid vicious retain loops.  This retain is balanced in
  // the -stopFetch method.
  [delegate_ retain];
  
  downloadedData_ = [[NSMutableData alloc] init];
  return YES;

CannotBeginFetch:

  if (failedSEL) {
    
    NSError *error = [NSError errorWithDomain:kGTMHTTPFetcherErrorDomain
                                         code:kGTMHTTPFetcherErrorDownloadFailed
                                     userInfo:nil];
    
    [[self retain] autorelease]; // in case the callback releases us

    [delegate performSelector:failedSEL_ 
                   withObject:self 
                   withObject:error];
  }
    
  return NO;
}

// Returns YES if this is in the process of fetching a URL, or waiting to 
// retry
- (BOOL)isFetching {
  return (connection_ != nil || retryTimer_ != nil); 
}

// Returns the status code set in connection:didReceiveResponse:
- (NSInteger)statusCode {
  
  NSInteger statusCode;
  
  if (response_ != nil 
    && [response_ respondsToSelector:@selector(statusCode)]) {
    
    statusCode = [(NSHTTPURLResponse *)response_ statusCode];
  } else {
    //  Default to zero, in hopes of hinting "Unknown" (we can't be
    //  sure that things are OK enough to use 200).
    statusCode = 0;
  }
  return statusCode;
}

// Cancel the fetch of the URL that's currently in progress.
- (void)stopFetching {
  [self destroyRetryTimer];

  if (connection_) {
    // in case cancelling the connection calls this recursively, we want
    // to ensure that we'll only release the connection and delegate once,
    // so first set connection_ to nil
    
    NSURLConnection* oldConnection = connection_;
    connection_ = nil;
    
    // this may be called in a callback from the connection, so use autorelease
    [oldConnection cancel];
    [oldConnection autorelease]; 

    // balance the retain done when the connection was opened
    [delegate_ release];
  }
}

- (void)retryFetch {
  
  id holdDelegate = [[delegate_ retain] autorelease];
  
  [self stopFetching];
  
  [self beginFetchWithDelegate:holdDelegate
             didFinishSelector:finishedSEL_
               didFailSelector:failedSEL_];
}

#pragma mark NSURLConnection Delegate Methods

//
// NSURLConnection Delegate Methods
//

// This method just says "follow all redirects", which _should_ be the default behavior,
// According to file:///Developer/ADC%20Reference%20Library/documentation/Cocoa/Conceptual/URLLoadingSystem
// but the redirects were not being followed until I added this method.  May be
// a bug in the NSURLConnection code, or the documentation.
//
// In OS X 10.4.8 and earlier, the redirect request doesn't
// get the original's headers and body. This causes POSTs to fail. 
// So we construct a new request, a copy of the original, with overrides from the
// redirect.
//
// Docs say that if redirectResponse is nil, just return the redirectRequest.

- (NSURLRequest *)connection:(NSURLConnection *)connection
             willSendRequest:(NSURLRequest *)redirectRequest
            redirectResponse:(NSURLResponse *)redirectResponse {

  if (redirectRequest && redirectResponse) {
    NSMutableURLRequest *newRequest = [[request_ mutableCopy] autorelease];
    // copy the URL
    NSURL *redirectURL = [redirectRequest URL];
    NSURL *url = [newRequest URL];
    
    // disallow scheme changes (say, from https to http)    
    NSString *redirectScheme = [url scheme];
    NSString *newScheme = [redirectURL scheme];
    NSString *newResourceSpecifier = [redirectURL resourceSpecifier];
    
    if ([redirectScheme caseInsensitiveCompare:@"http"] == NSOrderedSame
        && newScheme != nil
        && [newScheme caseInsensitiveCompare:@"https"] == NSOrderedSame) {
      
      // allow the change from http to https
      redirectScheme = newScheme; 
    }
    
    NSString *newUrlString = [NSString stringWithFormat:@"%@:%@",
      redirectScheme, newResourceSpecifier];
    
    NSURL *newURL = [NSURL URLWithString:newUrlString];
    [newRequest setURL:newURL];

    // any headers in the redirect override headers in the original.
    NSDictionary *redirectHeaders = [redirectRequest allHTTPHeaderFields];
    if (redirectHeaders) {
      NSEnumerator *enumerator = [redirectHeaders keyEnumerator];
      NSString *key;
      while (nil != (key = [enumerator nextObject])) {
        NSString *value = [redirectHeaders objectForKey:key];
        [newRequest setValue:value forHTTPHeaderField:key];
      }
    }
    redirectRequest = newRequest;
    
    // save cookies from the response
    [self handleCookiesForResponse:redirectResponse];
    
    // log the response we just received
    [self setResponse:redirectResponse];
    [self logFetchWithError:nil];

    // update the request for future logging
    [self setRequest:redirectRequest];
}
  return redirectRequest;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
  
  // this method is called when the server has determined that it
  // has enough information to create the NSURLResponse
  // it can be called multiple times, for example in the case of a 
  // redirect, so each time we reset the data.
  [downloadedData_ setLength:0];

  [self setResponse:response];

  // save cookies from the response
  [self handleCookiesForResponse:response];  
}


// handleCookiesForResponse: handles storage of cookies for responses passed to
// connection:willSendRequest:redirectResponse: and connection:didReceiveResponse:
- (void)handleCookiesForResponse:(NSURLResponse *)response {
  
  if (cookieStorageMethod_ == kGTMHTTPFetcherCookieStorageMethodSystemDefault) {
    
    // do nothing special for NSURLConnection's default storage mechanism
    
  } else if ([response respondsToSelector:@selector(allHeaderFields)]) {
    
    // grab the cookies from the header as NSHTTPCookies and store them either
    // into our static array or into the fetchHistory
    
    NSDictionary *responseHeaderFields = [(NSHTTPURLResponse *)response allHeaderFields];
    if (responseHeaderFields) {
      
      NSArray *cookies = [NSHTTPCookie cookiesWithResponseHeaderFields:responseHeaderFields
                                                                forURL:[response URL]]; 
      if ([cookies count] > 0) {
        
        NSMutableArray *cookieArray = nil;
        
        // static cookies are stored in gGTMFetcherStaticCookies; fetchHistory 
        // cookies are stored in fetchHistory_'s kGTMHTTPFetcherHistoryCookiesKey
        
        if (cookieStorageMethod_ == kGTMHTTPFetcherCookieStorageMethodStatic) {
          
          cookieArray = gGTMFetcherStaticCookies;
          
        } else if (cookieStorageMethod_ == kGTMHTTPFetcherCookieStorageMethodFetchHistory
                   && fetchHistory_ != nil) {
          
          cookieArray = [fetchHistory_ objectForKey:kGTMHTTPFetcherHistoryCookiesKey];
          if (cookieArray == nil) {
            cookieArray = [NSMutableArray array];
            [fetchHistory_ setObject:cookieArray forKey:kGTMHTTPFetcherHistoryCookiesKey];
          }
        }
        
        if (cookieArray) {
          @synchronized(cookieArray) {
            [self setCookies:cookies inArray:cookieArray];
          }
        }
      }
    }
  }
}

-(void)connection:(NSURLConnection *)connection
       didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
  
  if ([challenge previousFailureCount] <= 2) {
    
    NSURLCredential *credential = credential_;
    
    if ([[challenge protectionSpace] isProxy] && proxyCredential_ != nil) {
      credential = proxyCredential_;
    }
    
    // Here, if credential is still nil, then we *could* try to get it from
    // NSURLCredentialStorage's defaultCredentialForProtectionSpace:.
    // We don't, because we're assuming:
    //
    // - for server credentials, we only want ones supplied by the program
    //   calling http fetcher
    // - for proxy credentials, if one were necessary and available in the
    //   keychain, it would've been found automatically by NSURLConnection
    //   and this challenge delegate method never would've been called
    //   anyway
    
    if (credential) {
      // try the credential
      [[challenge sender] useCredential:credential
             forAuthenticationChallenge:challenge];
      return;
    } 
  }
  
  // If we don't have credentials, or we've already failed auth 3x...
  [[challenge sender] cancelAuthenticationChallenge:challenge];
  
  // report the error, putting the challenge as a value in the userInfo
  // dictionary
  NSDictionary *userInfo = [NSDictionary dictionaryWithObject:challenge
                                                       forKey:kGTMHTTPFetcherErrorChallengeKey];
  
  NSError *error = [NSError errorWithDomain:kGTMHTTPFetcherErrorDomain
                                       code:kGTMHTTPFetcherErrorAuthenticationChallengeFailed
                                   userInfo:userInfo];
  
  [self connection:connection didFailWithError:error];  
}



- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
  [downloadedData_ appendData:data];
  
  if (receivedDataSEL_) {
   [delegate_ performSelector:receivedDataSEL_
                   withObject:self
                   withObject:downloadedData_];
  }
}

- (void)updateFetchHistory {
  
  if (fetchHistory_) {
    
    NSString* urlString = [[request_ URL] absoluteString];
    if ([response_ respondsToSelector:@selector(allHeaderFields)]) {
      NSDictionary *headers = [(NSHTTPURLResponse *)response_ allHeaderFields];
      NSString* lastModifiedStr = [headers objectForKey:kGTMLastModifiedHeader];
    
      // get the dictionary mapping URLs to last-modified dates
      NSMutableDictionary* lastModifiedDict = [fetchHistory_ objectForKey:kGTMHTTPFetcherHistoryLastModifiedKey];
      if (!lastModifiedDict) {
        lastModifiedDict = [NSMutableDictionary dictionary];
        [fetchHistory_ setObject:lastModifiedDict forKey:kGTMHTTPFetcherHistoryLastModifiedKey];
      }

      NSMutableDictionary* datedDataCache = nil;
      if (shouldCacheDatedData_) {
        // get the dictionary mapping URLs to cached, dated data
        datedDataCache = [fetchHistory_ objectForKey:kGTMHTTPFetcherHistoryDatedDataKey];
        if (!datedDataCache) {
          datedDataCache = [NSMutableDictionary dictionary];
          [fetchHistory_ setObject:datedDataCache forKey:kGTMHTTPFetcherHistoryDatedDataKey];
        }
      }
      
      NSInteger statusCode = [self statusCode];
      if (statusCode != kGTMHTTPFetcherStatusNotModified) {
        
        // save this last modified date string for successful results (<300)
        // If there's no last modified string, clear the dictionary 
        // entry for this URL. Also cache or delete the data, if appropriate 
        // (when datedDataCache is non-nil.)
        if (lastModifiedStr && statusCode < 300) {
          [lastModifiedDict setValue:lastModifiedStr forKey:urlString];
          [datedDataCache setValue:downloadedData_ forKey:urlString];
        } else { 
          [lastModifiedDict removeObjectForKey:urlString];
          [datedDataCache removeObjectForKey:urlString];
        }
      }
    }
  }
}

// for error 304's ("Not Modified") where we've cached the data, return status 
// 200 ("OK") to the caller (but leave the fetcher status as 304) 
// and copy the cached data to downloadedData_.  
// For other errors or if there's no cached data, just return the actual status.
- (NSInteger)statusAfterHandlingNotModifiedError {
  
  NSInteger status = [self statusCode];
  if (status == kGTMHTTPFetcherStatusNotModified && shouldCacheDatedData_) {
    
    // get the dictionary of URLs and data
    NSString* urlString = [[request_ URL] absoluteString];
    
    NSDictionary* datedDataCache = [fetchHistory_ objectForKey:kGTMHTTPFetcherHistoryDatedDataKey];
    NSData* cachedData = [datedDataCache objectForKey:urlString];
    
    if (cachedData) {
      // copy our stored data, and forge the status to pass on to the delegate
      [downloadedData_ setData:cachedData];
      status = 200;
    }
  }
  return status;
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
  
  [self updateFetchHistory];

  [[self retain] autorelease]; // in case the callback releases us
  
  [self logFetchWithError:nil];
  
  NSInteger status = [self statusAfterHandlingNotModifiedError];
  
  if (status >= 300) {
    
    if ([self shouldRetryNowForStatus:status error:nil]) {
      
      [self beginRetryTimer];
      
    } else {
      // not retrying
      
      // did they want failure notifications?
      if (failedSEL_) {
        
        NSDictionary *userInfo =
          [NSDictionary dictionaryWithObject:downloadedData_
                                      forKey:kGTMHTTPFetcherStatusDataKey];
        NSError *error = [NSError errorWithDomain:kGTMHTTPFetcherStatusDomain 
                                             code:status
                                         userInfo:userInfo];

        [delegate_ performSelector:failedSEL_ 
                        withObject:self 
                        withObject:error];
      }
      // we're done fetching
      [self stopFetching];
    }
    
  } else if (finishedSEL_) {
    
    // successful http status (under 300)
    [delegate_ performSelector:finishedSEL_
                    withObject:self
                    withObject:downloadedData_];
    [self stopFetching];
  }
  
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error {

  [self logFetchWithError:error];
  
  if ([self shouldRetryNowForStatus:0 error:error]) {
    
    [self beginRetryTimer];
    
  } else {    
  
    if (failedSEL_) {
      [[self retain] autorelease]; // in case the callback releases us

      [delegate_ performSelector:failedSEL_ 
                      withObject:self 
                      withObject:error];
    }
    
    [self stopFetching];
  }
}

#pragma mark Retries

- (BOOL)isRetryError:(NSError *)error {
  
  struct retryRecord {
    NSString *const domain;
    int code;
  };
  
  struct retryRecord retries[] = {
    { kGTMHTTPFetcherStatusDomain, 408 }, // request timeout
    { kGTMHTTPFetcherStatusDomain, 503 }, // service unavailable
    { kGTMHTTPFetcherStatusDomain, 504 }, // request timeout
    { NSURLErrorDomain, NSURLErrorTimedOut },
    { NSURLErrorDomain, NSURLErrorNetworkConnectionLost },
    { nil, 0 }
  };

  // NSError's isEqual always returns false for equal but distinct instances
  // of NSError, so we have to compare the domain and code values explicitly

  for (int idx = 0; retries[idx].domain != nil; idx++) {
    
    if ([[error domain] isEqual:retries[idx].domain]
        && [error code] == retries[idx].code) {
     
      return YES;
    }
  }
  return NO;
}


// shouldRetryNowForStatus:error: returns YES if the user has enabled retries
// and the status or error is one that is suitable for retrying.  "Suitable"
// means either the isRetryError:'s list contains the status or error, or the
// user's retrySelector: is present and returns YES when called.
- (BOOL)shouldRetryNowForStatus:(NSInteger)status
                          error:(NSError *)error {
  
  if ([self isRetryEnabled]) {
    
    if ([self nextRetryInterval] < [self maxRetryInterval]) {
      
      if (error == nil) {
        // make an error for the status
       error = [NSError errorWithDomain:kGTMHTTPFetcherStatusDomain
                                   code:status
                               userInfo:nil]; 
      }
      
      BOOL willRetry = [self isRetryError:error];
      
      if (retrySEL_) {
        NSMethodSignature *signature = [delegate_ methodSignatureForSelector:retrySEL_];
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
        [invocation setSelector:retrySEL_];
        [invocation setTarget:delegate_];
        [invocation setArgument:&self atIndex:2];
        [invocation setArgument:&willRetry atIndex:3];
        [invocation setArgument:&error atIndex:4];
        [invocation invoke];
        
        [invocation getReturnValue:&willRetry];
      }
      
      return willRetry;
    }
  }
  
  return NO;
}

- (void)beginRetryTimer {
  
  NSTimeInterval nextInterval = [self nextRetryInterval];
  NSTimeInterval maxInterval = [self maxRetryInterval];

  NSTimeInterval newInterval = MIN(nextInterval, maxInterval);
    
  [self primeTimerWithNewTimeInterval:newInterval];
}

- (void)primeTimerWithNewTimeInterval:(NSTimeInterval)secs {
  
  [self destroyRetryTimer];
  
  lastRetryInterval_ = secs;
  
  retryTimer_ = [NSTimer scheduledTimerWithTimeInterval:secs
                                  target:self
                                selector:@selector(retryTimerFired:)
                                userInfo:nil
                                 repeats:NO];
  [retryTimer_ retain];
}

- (void)retryTimerFired:(NSTimer *)timer {

  [self destroyRetryTimer];
  
  retryCount_++;

  [self retryFetch];
}

- (void)destroyRetryTimer {
  
  [retryTimer_ invalidate];
  [retryTimer_ autorelease];
  retryTimer_ = nil;  
}

- (unsigned int)retryCount {
  return retryCount_; 
}

- (NSTimeInterval)nextRetryInterval {
  // the next wait interval is the factor (2.0) times the last interval,  
  // but never less than the minimum interval
  NSTimeInterval secs = lastRetryInterval_ * retryFactor_;
  secs = MIN(secs, maxRetryInterval_);
  secs = MAX(secs, minRetryInterval_);
  
  return secs;
}

- (BOOL)isRetryEnabled {
  return isRetryEnabled_;  
}

- (void)setIsRetryEnabled:(BOOL)flag {
  
  if (flag && !isRetryEnabled_) {
    // We defer initializing these until the user calls setIsRetryEnabled
    // to avoid seeding the random number generator if it's not needed.
    // However, it means min and max intervals for this fetcher are reset
    // as a side effect of calling setIsRetryEnabled.
    //
    // seed the random value, and make an initial retry interval
    // random between 1.0 and 2.0 seconds
    srandomdev(); 
    [self setMinRetryInterval:0.0]; 
    [self setMaxRetryInterval:kDefaultMaxRetryInterval];
    [self setRetryFactor:2.0];
    lastRetryInterval_ = 0.0;
  }
  isRetryEnabled_ = flag; 
}; 

- (SEL)retrySelector {
  return retrySEL_; 
}

- (void)setRetrySelector:(SEL)theSelector {
  retrySEL_ = theSelector;  
}

- (NSTimeInterval)maxRetryInterval {
  return maxRetryInterval_;  
}

- (void)setMaxRetryInterval:(NSTimeInterval)secs {
  if (secs > 0) {
    maxRetryInterval_ = secs; 
  } else {
    maxRetryInterval_ = kDefaultMaxRetryInterval; 
  }
}

- (double)minRetryInterval {
  return minRetryInterval_;  
}

- (void)setMinRetryInterval:(NSTimeInterval)secs {
  if (secs > 0) {
    minRetryInterval_ = secs; 
  } else {
    // set min interval to a random value between 1.0 and 2.0 seconds
    // so that if multiple clients start retrying at the same time, they'll
    // repeat at different times and avoid overloading the server
    minRetryInterval_ = 1.0 + ((double)(random() & 0x0FFFF) / (double) 0x0FFFF);
  }
}

- (double)retryFactor {
  return retryFactor_; 
}

- (void)setRetryFactor:(double)multiplier {
  retryFactor_ = multiplier; 
}

#pragma mark Getters and Setters

- (NSMutableURLRequest *)request {
  return request_;  
}

- (void)setRequest:(NSURLRequest *)theRequest {
  [request_ autorelease];
  request_ = [theRequest mutableCopy];
}

- (NSURLCredential *)credential {
  return credential_;
}

- (void)setCredential:(NSURLCredential *)theCredential {
  [credential_ autorelease];
  credential_ = [theCredential retain]; 
}

- (NSURLCredential *)proxyCredential {
  return proxyCredential_;
}

- (void)setProxyCredential:(NSURLCredential *)theCredential {
  [proxyCredential_ autorelease];
  proxyCredential_ = [theCredential retain]; 
}

- (NSData *)postData {
  return postData_; 
}

- (void)setPostData:(NSData *)theData {
  [postData_ autorelease]; 
  postData_ = [theData retain];
}

- (NSInputStream *)postStream {
  return postStream_; 
}

- (void)setPostStream:(NSInputStream *)theStream {
  [postStream_ autorelease]; 
  postStream_ = [theStream retain];
}

- (GTMHTTPFetcherCookieStorageMethod)cookieStorageMethod {
  return cookieStorageMethod_; 
}

- (void)setCookieStorageMethod:(GTMHTTPFetcherCookieStorageMethod)method {
  
  cookieStorageMethod_ = method; 
  
  if (method == kGTMHTTPFetcherCookieStorageMethodSystemDefault) {
    [request_ setHTTPShouldHandleCookies:YES];
  } else {
    [request_ setHTTPShouldHandleCookies:NO];
  }
}

- (id)delegate {
  return delegate_; 
}

- (void)setDelegate:(id)theDelegate {
  
  // we retain delegate_ only during the life of the connection
  if (connection_) {
    [delegate_ autorelease];
    delegate_ = [theDelegate retain];
  } else {
    delegate_ = theDelegate; 
  }
}

- (SEL)receivedDataSelector {
  return receivedDataSEL_; 
}

- (void)setReceivedDataSelector:(SEL)theSelector {
  receivedDataSEL_ = theSelector;  
}

- (NSURLResponse *)response {
  return response_;
}

- (void)setResponse:(NSURLResponse *)response {
  [response_ autorelease];
  response_ = [response retain];
}

- (NSMutableDictionary *)fetchHistory {
  return fetchHistory_;
}

- (void)setFetchHistory:(NSMutableDictionary *)fetchHistory {
  [fetchHistory_ autorelease];
  fetchHistory_ = [fetchHistory retain];
  
  if (fetchHistory_ != nil) {
    [self setCookieStorageMethod:kGTMHTTPFetcherCookieStorageMethodFetchHistory];
  } else {
    [self setCookieStorageMethod:kGTMHTTPFetcherCookieStorageMethodStatic];
  }
}

- (void)setShouldCacheDatedData:(BOOL)flag {
  shouldCacheDatedData_ = flag;
  if (!flag) {
    [self clearDatedDataHistory];
  }
}

- (BOOL)shouldCacheDatedData {
  return shouldCacheDatedData_; 
}

// delete last-modified dates and cached data from the fetch history
- (void)clearDatedDataHistory {
  [fetchHistory_ removeObjectForKey:kGTMHTTPFetcherHistoryLastModifiedKey];
  [fetchHistory_ removeObjectForKey:kGTMHTTPFetcherHistoryDatedDataKey]; 
}

- (id)userData {
  return userData_;
}

- (void)setUserData:(id)theObj {
  [userData_ autorelease]; 
  userData_ = [theObj retain];
}

- (NSArray *)runLoopModes {
  return runLoopModes_;
}

- (void)setRunLoopModes:(NSArray *)modes {
  [runLoopModes_ autorelease]; 
  runLoopModes_ = [modes retain];
}

+ (BOOL)doesSupportRunLoopModes {
  SEL sel = @selector(initWithRequest:delegate:startImmediately:);
  return [NSURLConnection instancesRespondToSelector:sel];
}

+ (NSArray *)defaultRunLoopModes {
  return gGTMFetcherDefaultRunLoopModes; 
}

+ (void)setDefaultRunLoopModes:(NSArray *)modes {
  [gGTMFetcherDefaultRunLoopModes autorelease];
  gGTMFetcherDefaultRunLoopModes = [modes retain];
}

+ (Class)connectionClass {
  if (gGTMFetcherConnectionClass == nil) {
    gGTMFetcherConnectionClass = [NSURLConnection class]; 
  }
  return gGTMFetcherConnectionClass; 
}

+ (void)setConnectionClass:(Class)theClass {
  gGTMFetcherConnectionClass = theClass;
}

#pragma mark Cookies

// return a cookie from the array with the same name, domain, and path as the 
// given cookie, or else return nil if none found
//
// Both the cookie being tested and all cookies in cookieStorageArray should
// be valid (non-nil name, domains, paths)
- (NSHTTPCookie *)cookieMatchingCookie:(NSHTTPCookie *)cookie
                               inArray:(NSArray *)cookieStorageArray {

  NSUInteger numberOfCookies = [cookieStorageArray count];
  NSString *name = [cookie name];
  NSString *domain = [cookie domain];
  NSString *path = [cookie path];
  
  _GTMDevAssert(name && domain && path,
                @"Invalid cookie (name:%@ domain:%@ path:%@)", 
                name, domain, path);
  
  for (NSUInteger idx = 0; idx < numberOfCookies; idx++) {
    
    NSHTTPCookie *storedCookie = [cookieStorageArray objectAtIndex:idx];

    if ([[storedCookie name] isEqual:name]
        && [[storedCookie domain] isEqual:domain]
        && [[storedCookie path] isEqual:path]) {
      
      return storedCookie; 
    }
  }
  return nil;
}

// remove any expired cookies from the array, excluding cookies with nil
// expirations
- (void)removeExpiredCookiesInArray:(NSMutableArray *)cookieStorageArray {
  
  // count backwards since we're deleting items from the array
  for (NSInteger idx = [cookieStorageArray count] - 1; idx >= 0; idx--) {
    
    NSHTTPCookie *storedCookie = [cookieStorageArray objectAtIndex:idx];
    
    NSDate *expiresDate = [storedCookie expiresDate];
    if (expiresDate && [expiresDate timeIntervalSinceNow] < 0) {
      [cookieStorageArray removeObjectAtIndex:idx];
    }
  }
}


// retrieve all cookies appropriate for the given URL, considering
// domain, path, cookie name, expiration, security setting.
// Side effect: removed expired cookies from the storage array
- (NSArray *)cookiesForURL:(NSURL *)theURL inArray:(NSMutableArray *)cookieStorageArray {
  
  [self removeExpiredCookiesInArray:cookieStorageArray];
  
  NSMutableArray *foundCookies = [NSMutableArray array];

  // we'll prepend "." to the desired domain, since we want the
  // actual domain "nytimes.com" to still match the cookie domain ".nytimes.com"
  // when we check it below with hasSuffix
  NSString *host = [theURL host];
  NSString *path = [theURL path];
  NSString *scheme = [theURL scheme];
  
  NSString *domain = nil;
  if ([host isEqual:@"localhost"]) {
    // the domain stored into NSHTTPCookies for localhost is "localhost.local"
    domain = @"localhost.local"; 
  } else {
    if (host) {
      domain = [@"." stringByAppendingString:host]; 
    }
  }
  
  NSUInteger numberOfCookies = [cookieStorageArray count];
  for (NSUInteger idx = 0; idx < numberOfCookies; idx++) {
    
    NSHTTPCookie *storedCookie = [cookieStorageArray objectAtIndex:idx];
    
    NSString *cookieDomain = [storedCookie domain];
    NSString *cookiePath = [storedCookie path];
    BOOL cookieIsSecure = [storedCookie isSecure];
    
    BOOL domainIsOK = [domain hasSuffix:cookieDomain];
    BOOL pathIsOK = [cookiePath isEqual:@"/"] || [path hasPrefix:cookiePath];
    BOOL secureIsOK = (!cookieIsSecure) || [scheme isEqual:@"https"];
    
    if (domainIsOK && pathIsOK && secureIsOK) {
      [foundCookies addObject:storedCookie];
    }
  }
  return foundCookies;
}

// return cookies for the given URL using the current cookie storage method
- (NSArray *)cookiesForURL:(NSURL *)theURL {
  
  NSArray *cookies = nil;
  NSMutableArray *cookieStorageArray = nil;
  
  if (cookieStorageMethod_ == kGTMHTTPFetcherCookieStorageMethodStatic) {
    cookieStorageArray = gGTMFetcherStaticCookies;
  } else if (cookieStorageMethod_ == kGTMHTTPFetcherCookieStorageMethodFetchHistory) {
    cookieStorageArray = [fetchHistory_ objectForKey:kGTMHTTPFetcherHistoryCookiesKey];
  } else {
    // kGTMHTTPFetcherCookieStorageMethodSystemDefault
    cookies = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookiesForURL:theURL];    
  }
  
  if (cookieStorageArray) {
    
    @synchronized(cookieStorageArray) {
      
      // cookiesForURL returns a new array of immutable NSCookie objects
      // from cookieStorageArray
      cookies = [self cookiesForURL:theURL
                            inArray:cookieStorageArray];
    }
  }
  return cookies;
}


// add all cookies in the array |newCookies| to the storage array,
// replacing cookies in the storage array as appropriate
// Side effect: removes expired cookies from the storage array
- (void)setCookies:(NSArray *)newCookies
           inArray:(NSMutableArray *)cookieStorageArray {
  
  [self removeExpiredCookiesInArray:cookieStorageArray];

  NSEnumerator *newCookieEnum = [newCookies objectEnumerator];
  NSHTTPCookie *newCookie;
  
  while ((newCookie = [newCookieEnum nextObject]) != nil) {
    
    if ([[newCookie name] length] > 0
        && [[newCookie domain] length] > 0
        && [[newCookie path] length] > 0) {

      // remove the cookie if it's currently in the array
      NSHTTPCookie *oldCookie = [self cookieMatchingCookie:newCookie
                                                   inArray:cookieStorageArray];
      if (oldCookie) {
        [cookieStorageArray removeObject:oldCookie];
      }
      
      // make sure the cookie hasn't already expired
      NSDate *expiresDate = [newCookie expiresDate];
      if ((!expiresDate) || [expiresDate timeIntervalSinceNow] > 0) {
        [cookieStorageArray addObject:newCookie];
      }
      
    } else {
      _GTMDevAssert(NO, @"Cookie incomplete: %@", newCookie); 
    }
  }
}
@end

#pragma mark Logging

// NOTE: Threads and Logging
//
// All the NSURLConnection callbacks happen on one thread, so we don't have
// to put any synchronization into the logging code.  Yes, the state around
// logging (it's directory, etc.) could use it, but for now, that's punted.


// We don't invoke Leopard methods on 10.4, because we check if the methods are
// implemented before invoking it, but we need to be able to compile without
// warnings.
// This declaration means if you target <=10.4, this method will compile
// without complaint in this source, so you must test with
// -respondsToSelector:, too.
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
@interface NSFileManager (LeopardMethodsOnTigerBuilds)
- (BOOL)removeItemAtPath:(NSString *)path error:(NSError **)error;
@end
#endif
// The iPhone Foundation removes the deprecated removeFileAtPath:handler:
#if GTM_IPHONE_SDK
@interface NSFileManager (TigerMethodsOniPhoneBuilds)
- (BOOL)removeFileAtPath:(NSString *)path handler:(id)handler;
@end
#endif

@implementation GTMHTTPFetcher (GTMHTTPFetcherLogging)

// if GTM_HTTPFETCHER_ENABLE_LOGGING is defined by the user's project then
// logging code will be compiled into the framework

#if !GTM_HTTPFETCHER_ENABLE_LOGGING
- (void)logFetchWithError:(NSError *)error {}

+ (void)setLoggingDirectory:(NSString *)path {}
+ (NSString *)loggingDirectory {return nil;}

+ (void)setIsLoggingEnabled:(BOOL)flag {}
+ (BOOL)isLoggingEnabled {return NO;}

+ (void)setLoggingProcessName:(NSString *)str {}
+ (NSString *)loggingProcessName {return nil;}

+ (void)setLoggingDateStamp:(NSString *)str {}
+ (NSString *)loggingDateStamp {return nil;}

- (void)appendLoggedStreamData:(NSData *)newData {}
- (void)logCapturePostStream {}
#else // GTM_HTTPFETCHER_ENABLE_LOGGING

// fetchers come and fetchers go, but statics are forever
static BOOL gIsLoggingEnabled = NO;
static NSString *gLoggingDirectoryPath = nil;
static NSString *gLoggingDateStamp = nil;
static NSString* gLoggingProcessName = nil; 

+ (void)setLoggingDirectory:(NSString *)path {
  [gLoggingDirectoryPath autorelease];
  gLoggingDirectoryPath = [path copy];
}

+ (NSString *)loggingDirectory {
  
  if (!gLoggingDirectoryPath) {
    
#if GTM_IPHONE_SDK
    // default to a directory called GTMHTTPDebugLogs into a sandbox-safe
    // directory that a devloper can find easily, the application home
    NSArray *arr = [NSArray arrayWithObject:NSHomeDirectory()];
#else
    // default to a directory called GTMHTTPDebugLogs in the desktop folder
    NSArray *arr = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory,
                                                       NSUserDomainMask, YES);
#endif
    
    if ([arr count] > 0) {
      NSString *const kGTMLogFolderName = @"GTMHTTPDebugLogs";
      
      NSString *desktopPath = [arr objectAtIndex:0];
      NSString *logsFolderPath = [desktopPath stringByAppendingPathComponent:kGTMLogFolderName];
      
      BOOL doesFolderExist;
      BOOL isDir = NO;
      NSFileManager *fileManager = [NSFileManager defaultManager];
      doesFolderExist = [fileManager fileExistsAtPath:logsFolderPath 
                                          isDirectory:&isDir];
      
      if (!doesFolderExist) {
        // make the directory
        doesFolderExist = [fileManager createDirectoryAtPath:logsFolderPath 
                                                  attributes:nil];
      }
      
      if (doesFolderExist) {
        // it's there; store it in the global
        gLoggingDirectoryPath = [logsFolderPath copy];
      }
    }
  }
  return gLoggingDirectoryPath;
}

+ (void)setIsLoggingEnabled:(BOOL)flag {
  gIsLoggingEnabled = flag;
}

+ (BOOL)isLoggingEnabled {
  return gIsLoggingEnabled;
}

+ (void)setLoggingProcessName:(NSString *)str {
  [gLoggingProcessName release];
  gLoggingProcessName = [str copy];
}

+ (NSString *)loggingProcessName {
  
  // get the process name (once per run) replacing spaces with underscores
  if (!gLoggingProcessName) {
    
    NSString *procName = [[NSProcessInfo processInfo] processName];
    NSMutableString *loggingProcessName;
    loggingProcessName = [[NSMutableString alloc] initWithString:procName];
    
    [loggingProcessName replaceOccurrencesOfString:@" " 
                                        withString:@"_" 
                                           options:0 
                                             range:NSMakeRange(0, [gLoggingProcessName length])];
    gLoggingProcessName = loggingProcessName;
  } 
  return gLoggingProcessName;
}

+ (void)setLoggingDateStamp:(NSString *)str {
  [gLoggingDateStamp release];
  gLoggingDateStamp = [str copy];
}

+ (NSString *)loggingDateStamp {
  // we'll pick one date stamp per run, so a run that starts at a later second
  // will get a unique results html file
  if (!gLoggingDateStamp) {    
    // produce a string like 08-21_01-41-23PM
    
    NSDateFormatter *formatter = [[[NSDateFormatter alloc] init] autorelease];
    [formatter setFormatterBehavior:NSDateFormatterBehavior10_4];
    [formatter setDateFormat:@"M-dd_hh-mm-ssa"];
    
    gLoggingDateStamp = [[formatter stringFromDate:[NSDate date]] retain] ;    
  }
  return gLoggingDateStamp;
}

- (NSString *)cleanParameterFollowing:(NSString *)paramName
                           fromString:(NSString *)originalStr {
  // We don't want the password written to disk 
  //
  // find "&Passwd=" in the string, and replace it and the stuff that
  // follows it with "Passwd=_snip_"
  
  NSRange passwdRange = [originalStr rangeOfString:@"&Passwd="];
  if (passwdRange.location != NSNotFound) {
    
    // we found Passwd=; find the & that follows the parameter
    NSUInteger origLength = [originalStr length];
    NSRange restOfString = NSMakeRange(passwdRange.location+1, 
                                       origLength - passwdRange.location - 1);
    NSRange rangeOfFollowingAmp = [originalStr rangeOfString:@"&"
                                                     options:0
                                                       range:restOfString];
    NSRange replaceRange;
    if (rangeOfFollowingAmp.location == NSNotFound) {
      // found no other & so replace to end of string
      replaceRange = NSMakeRange(passwdRange.location, 
                                 rangeOfFollowingAmp.location - passwdRange.location);
    } else {
      // another parameter after &Passwd=foo
      replaceRange = NSMakeRange(passwdRange.location, 
                                 rangeOfFollowingAmp.location - passwdRange.location);
    }
    
    NSMutableString *result = [NSMutableString stringWithString:originalStr];
    NSString *replacement = [NSString stringWithFormat:@"%@_snip_", paramName];
    
    [result replaceCharactersInRange:replaceRange withString:replacement];
    return result;
  }
  return originalStr;
}

// stringFromStreamData creates a string given the supplied data
//
// If NSString can create a UTF-8 string from the data, then that is returned.
//
// Otherwise, this routine tries to find a MIME boundary at the beginning of
// the data block, and uses that to break up the data into parts. Each part
// will be used to try to make a UTF-8 string.  For parts that fail, a 
// replacement string showing the part header and <<n bytes>> is supplied
// in place of the binary data.

- (NSString *)stringFromStreamData:(NSData *)data {
  
  if (data == nil) return nil;
  
  // optimistically, see if the whole data block is UTF-8
  NSString *streamDataStr = [[[NSString alloc] initWithData:data
                                                   encoding:NSUTF8StringEncoding] autorelease];
  if (streamDataStr) return streamDataStr;
  
  // Munge a buffer by replacing non-ASCII bytes with underscores,
  // and turn that munged buffer an NSString.  That gives us a string
  // we can use with NSScanner.
  NSMutableData *mutableData = [NSMutableData dataWithData:data];
  unsigned char *bytes = [mutableData mutableBytes];
  
  for (NSUInteger idx = 0; idx < [mutableData length]; idx++) {
    if (bytes[idx] > 0x7F || bytes[idx] == 0) {
      bytes[idx] = '_';
    }
  }
  
  NSString *mungedStr = [[[NSString alloc] initWithData:mutableData
                                               encoding:NSUTF8StringEncoding] autorelease];
  if (mungedStr != nil) {
    
    // scan for the boundary string
    NSString *boundary = nil;
    NSScanner *scanner = [NSScanner scannerWithString:mungedStr];
    
    if ([scanner scanUpToString:@"\r\n" intoString:&boundary]
        && [boundary hasPrefix:@"--"]) {
      
      // we found a boundary string; use it to divide the string into parts
      NSArray *mungedParts = [mungedStr componentsSeparatedByString:boundary];
      
      // look at each of the munged parts in the original string, and try to 
      // convert those into UTF-8
      NSMutableArray *origParts = [NSMutableArray array];
      NSUInteger offset = 0;
      for (NSUInteger partIdx = 0; partIdx < [mungedParts count]; partIdx++) {
        
        NSString *mungedPart = [mungedParts objectAtIndex:partIdx];
        NSUInteger partSize = [mungedPart length];
        
        NSRange range = NSMakeRange(offset, partSize);
        NSData *origPartData = [data subdataWithRange:range];
        
        NSString *origPartStr = [[[NSString alloc] initWithData:origPartData
                                                       encoding:NSUTF8StringEncoding] autorelease];
        if (origPartStr) {
          // we could make this original part into UTF-8; use the string
          [origParts addObject:origPartStr];
        } else {
          // this part can't be made into UTF-8; scan the header, if we can
          NSString *header = nil;
          NSScanner *headerScanner = [NSScanner scannerWithString:mungedPart];
          if (![headerScanner scanUpToString:@"\r\n\r\n" intoString:&header]) {
            // we couldn't find a header
            header = @"";
          }
          
          // make a part string with the header and <<n bytes>>
          NSString *binStr = [NSString stringWithFormat:@"\r%@\r<<%u bytes>>\r",
                              header, partSize - [header length]];
          [origParts addObject:binStr];
        }
        offset += partSize + [boundary length];
      }
      
      // rejoin the original parts
      streamDataStr = [origParts componentsJoinedByString:boundary];
    }
  }  
  
  if (!streamDataStr) {
    // give up; just make a string showing the uploaded bytes
    streamDataStr = [NSString stringWithFormat:@"<<%u bytes>>", [data length]];
  }
  return streamDataStr;
}

// logFetchWithError is called following a successful or failed fetch attempt
//
// This method does all the work for appending to and creating log files

- (void)logFetchWithError:(NSError *)error {
  
  if (![[self class] isLoggingEnabled]) return;
  
  NSFileManager *fileManager = [NSFileManager defaultManager];
  
  // TODO:  add Javascript to display response data formatted in hex
  
  NSString *logDirectory = [[self class] loggingDirectory];
  NSString *processName = [[self class] loggingProcessName];
  NSString *dateStamp = [[self class] loggingDateStamp];
  
  // each response's NSData goes into its own xml or txt file, though all
  // responses for this run of the app share a main html file.  This 
  // counter tracks all fetch responses for this run of the app.
  static int zResponseCounter = 0; 
  zResponseCounter++;
  
  // file name for the html file containing plain text in a <textarea>
  NSString *responseDataUnformattedFileName = nil; 
  
  // file name for the "formatted" (raw) data file
  NSString *responseDataFormattedFileName = nil; 
  NSUInteger responseDataLength = [downloadedData_ length];
  
  NSURLResponse *response = [self response];
  NSString *responseBaseName = nil;
  
  // if there's response data, decide what kind of file to put it in based  
  // on the first bytes of the file or on the mime type supplied by the server
  if (responseDataLength) {
    NSString *responseDataExtn = nil;
    
    // generate a response file base name like
    //   SyncProto_http_response_10-16_01-56-58PM_3
    responseBaseName = [NSString stringWithFormat:@"%@_http_response_%@_%d",
                        processName, dateStamp, zResponseCounter];
    
    NSString *dataStr = [[[NSString alloc] initWithData:downloadedData_ 
                                               encoding:NSUTF8StringEncoding] autorelease];
    if (dataStr) {
      // we were able to make a UTF-8 string from the response data
      
      NSCharacterSet *whitespaceSet = [NSCharacterSet whitespaceCharacterSet];
      dataStr = [dataStr stringByTrimmingCharactersInSet:whitespaceSet];
      
      // save a plain-text version of the response data in an html cile  
      // containing a wrapped, scrollable <textarea>
      //
      // we'll use <textarea rows="33" cols="108" readonly=true wrap=soft>
      //   </textarea>  to fit inside our iframe
      responseDataUnformattedFileName = [responseBaseName stringByAppendingPathExtension:@"html"];
      NSString *textFilePath = [logDirectory stringByAppendingPathComponent:responseDataUnformattedFileName];
      
      NSString* wrapFmt = @"<textarea rows=\"33\" cols=\"108\" readonly=true"
      " wrap=soft>\n%@\n</textarea>";
      NSString* wrappedStr = [NSString stringWithFormat:wrapFmt, dataStr];
      [wrappedStr writeToFile:textFilePath 
                   atomically:NO 
                     encoding:NSUTF8StringEncoding 
                        error:nil];
      
      // now determine the extension for the "formatted" file, which is really 
      // the raw data written with an appropriate extension
      
      // for known file types, we'll write the data to a file with the
      // appropriate extension
      if ([dataStr hasPrefix:@"<?xml"]) {
        responseDataExtn = @"xml";
      } else if ([dataStr hasPrefix:@"<html"]) {
        responseDataExtn = @"html";
      } else {
        // add more types of identifiable text here
      }
      
    } else if ([[response MIMEType] isEqual:@"image/jpeg"]) {
      responseDataExtn = @"jpg";
    } else if ([[response MIMEType] isEqual:@"image/gif"]) {
      responseDataExtn = @"gif";
    } else if ([[response MIMEType] isEqual:@"image/png"]) {
      responseDataExtn = @"png";
    } else {
      // add more non-text types here
    }
    
    // if we have an extension, save the raw data in a file with that 
    // extension to be our "formatted" display file
    if (responseDataExtn) {
      responseDataFormattedFileName = [responseBaseName stringByAppendingPathExtension:responseDataExtn];
      NSString *formattedFilePath = [logDirectory stringByAppendingPathComponent:responseDataFormattedFileName];
      
      [downloadedData_ writeToFile:formattedFilePath atomically:NO];
    }
  }
  
  // we'll have one main html file per run of the app
  NSString *htmlName = [NSString stringWithFormat:@"%@_http_log_%@.html", 
                        processName, dateStamp];
  NSString *htmlPath =[logDirectory stringByAppendingPathComponent:htmlName];
  
  // if the html file exists (from logging previous fetches) we don't need
  // to re-write the header or the scripts
  BOOL didFileExist = [fileManager fileExistsAtPath:htmlPath];
  
  NSMutableString* outputHTML = [NSMutableString string];
  NSURLRequest *request = [self request];
  
  // we need file names for the various div's that we're going to show and hide,
  // names unique to this response's bundle of data, so we format our div
  // names with the counter that we incremented earlier
  NSString *requestHeadersName = [NSString stringWithFormat:@"RequestHeaders%d", zResponseCounter];
  NSString *postDataName = [NSString stringWithFormat:@"PostData%d", zResponseCounter];
  
  NSString *responseHeadersName = [NSString stringWithFormat:@"ResponseHeaders%d", zResponseCounter];
  NSString *responseDataDivName = [NSString stringWithFormat:@"ResponseData%d", zResponseCounter];
  NSString *dataIFrameID = [NSString stringWithFormat:@"DataIFrame%d", zResponseCounter];
  
  // we need a header to say we'll have UTF-8 text
  if (!didFileExist) {
    [outputHTML appendFormat:@"<html><head><meta http-equiv=\"content-type\" "
     "content=\"text/html; charset=UTF-8\"><title>%@ HTTP fetch log %@</title>",
     processName, dateStamp];
  }
  
  // write style sheets for each hideable element; each style sheet is 
  // customized with our current response number, since they'll share
  // the html page with other responses
  NSString *styleFormat = @"<style type=\"text/css\">div#%@ "
  "{ margin: 0px 20px 0px 20px; display: none; }</style>\n";
  
  [outputHTML appendFormat:styleFormat, requestHeadersName];
  [outputHTML appendFormat:styleFormat, postDataName];
  [outputHTML appendFormat:styleFormat, responseHeadersName];
  [outputHTML appendFormat:styleFormat, responseDataDivName];
  
  if (!didFileExist) {
    // write javascript functions.  The first one shows/hides the layer 
    // containing the iframe.
    NSString *scriptFormat = @"<script type=\"text/javascript\"> "
    "function toggleLayer(whichLayer){ var style2 = document.getElementById(whichLayer).style; "
    "style2.display = style2.display ? \"\":\"block\";}</script>\n";
    [outputHTML appendFormat:scriptFormat];
    
    // the second function is passed the src file; if it's what's shown, it 
    // toggles the iframe's visibility. If some other src is shown, it shows 
    // the iframe and loads the new source.  Note we want to load the source 
    // whenever we show the iframe too since Firefox seems to format it wrong 
    // when showing it if we don't reload it.
    NSString *toggleIFScriptFormat = @"<script type=\"text/javascript\"> "
    "function toggleIFrame(whichLayer,iFrameID,newsrc)"
    "{ \n var iFrameElem=document.getElementById(iFrameID); "
    "if (iFrameElem.src.indexOf(newsrc) != -1) { toggleLayer(whichLayer); } "
    "else { document.getElementById(whichLayer).style.display=\"block\"; } "
    "iFrameElem.src=newsrc; }</script>\n</head>\n<body>\n";
    [outputHTML appendFormat:toggleIFScriptFormat];
  }
  
  // now write the visible html elements
  
  // write the date & time
  [outputHTML appendFormat:@"<b>%@</b><br>", [[NSDate date] description]];
  
  // write the request URL
  [outputHTML appendFormat:@"<b>request:</b> %@ <i>URL:</i> <code>%@</code><br>\n",
   [request HTTPMethod], [request URL]];
  
  // write the request headers, toggleable
  NSDictionary *requestHeaders = [request allHTTPHeaderFields];
  if ([requestHeaders count]) {
    NSString *requestHeadersFormat = @"<a href=\"javascript:toggleLayer('%@');\">"
    "request headers (%d)</a><div id=\"%@\"><pre>%@</pre></div><br>\n";
    [outputHTML appendFormat:requestHeadersFormat,
     requestHeadersName, // layer name
     [requestHeaders count],
     requestHeadersName,
     [requestHeaders description]]; // description gives a human-readable dump
  } else {
    [outputHTML appendString:@"<i>Request headers: none</i><br>"];
  }
  
  // write the request post data, toggleable
  NSData *postData = postData_;
  if (loggedStreamData_) { 
    postData = loggedStreamData_;
  }
  
  if ([postData length]) {
    NSString *postDataFormat = @"<a href=\"javascript:toggleLayer('%@');\">"
    "posted data (%d bytes)</a><div id=\"%@\">%@</div><br>\n";
    NSString *postDataStr = [self stringFromStreamData:postData];
    if (postDataStr) {
      NSString *postDataTextAreaFmt = @"<pre>%@</pre>";
      if ([postDataStr rangeOfString:@"<"].location != NSNotFound) {
        postDataTextAreaFmt =  @"<textarea rows=\"15\" cols=\"100\""
        " readonly=true wrap=soft>\n%@\n</textarea>";
      } 
      NSString *cleanedPostData = [self cleanParameterFollowing:@"&Passwd=" 
                                                     fromString:postDataStr];
      NSString *postDataTextArea = [NSString stringWithFormat:
                                    postDataTextAreaFmt,  cleanedPostData];
      
      [outputHTML appendFormat:postDataFormat,
       postDataName, // layer name
       [postData length],
       postDataName,
       postDataTextArea];
    }
  } else {
    // no post data
  }
  
  // write the response status, MIME type, URL
  if (response) {
    NSString *statusString = @"";
    if ([response respondsToSelector:@selector(statusCode)]) {
      NSInteger status = [(NSHTTPURLResponse *)response statusCode];
      statusString = @"200";
      if (status != 200) {
        // purple for errors
        statusString = [NSString stringWithFormat:@"<FONT COLOR=\"#FF00FF\">%d</FONT>",
                        status];
      }
    }
    
    // show the response URL only if it's different from the request URL
    NSString *responseURLStr =  @"";
    NSURL *responseURL = [response URL];
    
    if (responseURL && ![responseURL isEqual:[request URL]]) {
      NSString *responseURLFormat = @"<br><FONT COLOR=\"#FF00FF\">response URL:"
      "</FONT> <code>%@</code>";
      responseURLStr = [NSString stringWithFormat:responseURLFormat,
                        [responseURL absoluteString]];
    }
    
    NSDictionary *responseHeaders = nil;
    if ([response respondsToSelector:@selector(allHeaderFields)]) {
      responseHeaders = [(NSHTTPURLResponse *)response allHeaderFields];
    }
    [outputHTML appendFormat:@"<b>response:</b> <i>status:</i> %@ <i>  "
     "&nbsp;&nbsp;&nbsp;MIMEType:</i><code> %@</code>%@<br>\n",
     statusString,
     [response MIMEType], 
     responseURLStr,
     responseHeaders ? [responseHeaders description] : @""];
    
    // write the response headers, toggleable
    if ([responseHeaders count]) {
      
      NSString *cookiesSet = [responseHeaders objectForKey:@"Set-Cookie"];
      
      NSString *responseHeadersFormat = @"<a href=\"javascript:toggleLayer("
      "'%@');\">response headers (%d)  %@</a><div id=\"%@\"><pre>%@</pre>"
      "</div><br>\n";
      [outputHTML appendFormat:responseHeadersFormat,
       responseHeadersName,
       [responseHeaders count],
       (cookiesSet ? @"<i>sets cookies</i>" : @""),
       responseHeadersName,
       [responseHeaders description]];
      
    } else {
      [outputHTML appendString:@"<i>Response headers: none</i><br>\n"];
    }
  }
  
  // error
  if (error) {
    [outputHTML appendFormat:@"<b>error:</b> %@ <br>\n", [error description]];
  }
  
  // write the response data.  We have links to show formatted and text
  //   versions, but they both show it in the same iframe, and both
  //   links also toggle visible/hidden
  if (responseDataFormattedFileName || responseDataUnformattedFileName) {
    
    // response data, toggleable links -- formatted and text versions
    if (responseDataFormattedFileName) {
      [outputHTML appendFormat:@"response data (%d bytes) formatted <b>%@</b> ",
       responseDataLength, 
       [responseDataFormattedFileName pathExtension]];
      
      // inline (iframe) link
      NSString *responseInlineFormattedDataNameFormat = @"&nbsp;&nbsp;<a "
      "href=\"javascript:toggleIFrame('%@','%@','%@');\">inline</a>\n";
      [outputHTML appendFormat:responseInlineFormattedDataNameFormat,
       responseDataDivName, // div ID
       dataIFrameID, // iframe ID (for reloading)
       responseDataFormattedFileName]; // src to reload 
      
      // plain link (so the user can command-click it into another tab)
      [outputHTML appendFormat:@"&nbsp;&nbsp;<a href=\"%@\">stand-alone</a><br>\n",
       [responseDataFormattedFileName
        stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
    }
    if (responseDataUnformattedFileName) {
      [outputHTML appendFormat:@"response data (%d bytes) plain text ",
       responseDataLength];
      
      // inline (iframe) link
      NSString *responseInlineDataNameFormat = @"&nbsp;&nbsp;<a href=\""
      "javascript:toggleIFrame('%@','%@','%@');\">inline</a> \n";
      [outputHTML appendFormat:responseInlineDataNameFormat,
       responseDataDivName, // div ID
       dataIFrameID, // iframe ID (for reloading)
       responseDataUnformattedFileName]; // src to reload 
      
      // plain link (so the user can command-click it into another tab)
      [outputHTML appendFormat:@"&nbsp;&nbsp;<a href=\"%@\">stand-alone</a><br>\n",
       [responseDataUnformattedFileName
        stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
    }
    
    // make the iframe
    NSString *divHTMLFormat = @"<div id=\"%@\">%@</div><br>\n";
    NSString *src = responseDataFormattedFileName ?  
    responseDataFormattedFileName : responseDataUnformattedFileName;
    NSString *escapedSrc = [src stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    NSString *iframeFmt = @" <iframe src=\"%@\" id=\"%@\" width=800 height=400>"
    "\n<a href=\"%@\">%@</a>\n </iframe>\n";
    NSString *dataIFrameHTML = [NSString stringWithFormat:iframeFmt,
                                escapedSrc, dataIFrameID, escapedSrc, src];
    [outputHTML appendFormat:divHTMLFormat, 
     responseDataDivName, dataIFrameHTML];
  } else {
    // could not parse response data; just show the length of it
    [outputHTML appendFormat:@"<i>Response data: %d bytes </i>\n", 
     responseDataLength];
  }
  
  [outputHTML appendString:@"<br><hr><p>"];
  
  // append the HTML to the main output file
  const char* htmlBytes = [outputHTML UTF8String];
  NSOutputStream *stream = [NSOutputStream outputStreamToFileAtPath:htmlPath 
                                                             append:YES];
  [stream open];
  [stream write:(const uint8_t *) htmlBytes maxLength:strlen(htmlBytes)];
  [stream close];
  
  // make a symlink to the latest html
  NSString *symlinkName = [NSString stringWithFormat:@"%@_http_log_newest.html", 
                           processName];
  NSString *symlinkPath = [logDirectory stringByAppendingPathComponent:symlinkName];
  
  // removeFileAtPath might be going away, but removeItemAtPath does not exist
  // in 10.4
  if ([fileManager respondsToSelector:@selector(removeFileAtPath:handler:)]) {
    [fileManager removeFileAtPath:symlinkPath handler:nil];
  } else if ([fileManager respondsToSelector:@selector(removeItemAtPath:error:)]) {
    // To make the next line compile when targeting 10.4, we declare
    // removeItemAtPath:error: in an @interface above
    [fileManager removeItemAtPath:symlinkPath error:NULL];
  }
  
  [fileManager createSymbolicLinkAtPath:symlinkPath pathContent:htmlPath];
}

- (void)logCapturePostStream {

#if GTM_HTTPFETCHER_ENABLE_INPUTSTREAM_LOGGING
  // This is called when beginning a fetch.  The caller should have already
  // verified that logging is enabled, and should have allocated 
  // loggedStreamData_ as a mutable object.
  
  // If we're logging, we need to wrap the upload stream with our monitor
  // stream subclass that will call us back with the bytes being read from the
  // stream
  
  // our wrapper will retain the old post stream
  [postStream_ autorelease];
  
  // length can be 
  postStream_ = [GTMInputStreamLogger inputStreamWithStream:postStream_
                                                     length:0];
  [postStream_ retain];

  // we don't really want monitoring callbacks; our subclass will be
  // calling our appendLoggedStreamData: method at every read instead
  [(GTMInputStreamLogger *)postStream_ setMonitorDelegate:self
                                                 selector:nil];
#endif // GTM_HTTPFETCHER_ENABLE_INPUTSTREAM_LOGGING
}

- (void)appendLoggedStreamData:(NSData *)newData {
  [loggedStreamData_ appendData:newData];
}

#endif // GTM_HTTPFETCHER_ENABLE_LOGGING
@end

#if GTM_HTTPFETCHER_ENABLE_INPUTSTREAM_LOGGING
@implementation GTMInputStreamLogger
- (NSInteger)read:(uint8_t *)buffer maxLength:(NSUInteger)len {
  
  // capture the read stream data, and pass it to the delegate to append to
  NSInteger result = [super read:buffer maxLength:len];
  if (result >= 0) {
    NSData *data = [NSData dataWithBytes:buffer length:result];
    [monitorDelegate_ appendLoggedStreamData:data];
  }
  return result;
}
@end
#endif // GTM_HTTPFETCHER_ENABLE_INPUTSTREAM_LOGGING
