//
//  GTMHTTPFetcherTest.m
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

#import "GTMSenTestCase.h"
#import "GTMHTTPFetcher.h"
#import "GTMTestHTTPServer.h"

@interface GTMHTTPFetcherTest : GTMTestCase {
  // these ivars are checked after fetches, and are reset by resetFetchResponse
  NSData *fetchedData_;
  NSError *fetcherError_;
  NSInteger fetchedStatus_;
  NSURLResponse *fetchedResponse_;
  NSMutableURLRequest *fetchedRequest_;
  
  // setup/teardown ivars
  NSMutableDictionary *fetchHistory_;
  GTMTestHTTPServer *testServer_;
}
@end

@interface GTMHTTPFetcherTest (PrivateMethods)
- (GTMHTTPFetcher *)doFetchWithURLString:(NSString *)urlString
                        cachingDatedData:(BOOL)doCaching;

- (GTMHTTPFetcher *)doFetchWithURLString:(NSString *)urlString 
                        cachingDatedData:(BOOL)doCaching 
                           retrySelector:(SEL)retrySel
                        maxRetryInterval:(NSTimeInterval)maxRetryInterval
                                userData:(id)userData;

- (NSString *)fileURLStringToTestFileName:(NSString *)name;
- (BOOL)countRetriesFetcher:(GTMHTTPFetcher *)fetcher
                  willRetry:(BOOL)suggestedWillRetry
                   forError:(NSError *)error;
- (BOOL)fixRequestFetcher:(GTMHTTPFetcher *)fetcher
                willRetry:(BOOL)suggestedWillRetry
                 forError:(NSError *)error;
- (void)testFetcher:(GTMHTTPFetcher *)fetcher finishedWithData:(NSData *)data;
- (void)testFetcher:(GTMHTTPFetcher *)fetcher failedWithError:(NSError *)error;
@end

@implementation GTMHTTPFetcherTest

static const NSTimeInterval kRunLoopInterval = 0.01; 
//  The bogus-fetch test can take >10s to pass. Pick something way higher
//  to avoid failing.
static const NSTimeInterval kGiveUpInterval = 60.0; // bail on the test if 60 seconds elapse

static NSString *const kValidFileName = @"GTMHTTPFetcherTestPage.html";

- (void)setUp {
  fetchHistory_ = [[NSMutableDictionary alloc] init];
  
  NSBundle *testBundle = [NSBundle bundleForClass:[self class]];
  STAssertNotNil(testBundle, nil);
  NSString *docRoot = [testBundle pathForResource:@"GTMHTTPFetcherTestPage"
                                           ofType:@"html"];
  docRoot = [docRoot stringByDeletingLastPathComponent];
  STAssertNotNil(docRoot, nil);
  
  testServer_ = [[GTMTestHTTPServer alloc] initWithDocRoot:docRoot];
  STAssertNotNil(testServer_, @"failed to create a testing server");
}

- (void)resetFetchResponse {
  [fetchedData_ release];
  fetchedData_ = nil;
  
  [fetcherError_ release];
  fetcherError_ = nil;
  
  [fetchedRequest_ release];
  fetchedRequest_ = nil;
  
  [fetchedResponse_ release];
  fetchedResponse_ = nil;
  
  fetchedStatus_ = 0;
}

- (void)tearDown {
  [testServer_ release];
  testServer_ = nil;

  [self resetFetchResponse];
  
  [fetchHistory_ release];
  fetchHistory_ = nil;
}

- (void)testValidFetch {
  
  NSString *urlString = [self fileURLStringToTestFileName:kValidFileName];
  
  [self doFetchWithURLString:urlString cachingDatedData:YES];
  
  STAssertNotNil(fetchedData_,
                 @"failed to fetch data, status:%ld error:%@, URL:%@",
                 (long)fetchedStatus_, fetcherError_, urlString);  
  STAssertNotNil(fetchedResponse_,
                 @"failed to get fetch response, status:%ld error:%@",
                 (long)fetchedStatus_, fetcherError_);  
  STAssertNotNil(fetchedRequest_, @"failed to get fetch request, URL %@", 
                 urlString);  
  STAssertNil(fetcherError_, @"fetching data gave error: %@", fetcherError_);  
  STAssertEquals(fetchedStatus_, (NSInteger)200,
                 @"fetching data expected status 200, instead got %ld, for URL %@",
                 (long)fetchedStatus_, urlString);
  
  // no cookies should be sent with our first request
  NSDictionary *headers = [fetchedRequest_ allHTTPHeaderFields];
  NSString *cookiesSent = [headers objectForKey:@"Cookie"];
  STAssertNil(cookiesSent, @"Cookies sent unexpectedly: %@", cookiesSent);
  
  
  // cookies should have been set by the response; specifically, TestCookie
  // should be set to the name of the file requested
  NSDictionary *responseHeaders;
  
  responseHeaders = [(NSHTTPURLResponse *)fetchedResponse_ allHeaderFields];
  NSString *cookiesSetString = [responseHeaders objectForKey:@"Set-Cookie"];
  NSString *cookieExpected = [NSString stringWithFormat:@"TestCookie=%@",
    kValidFileName];
  STAssertEqualObjects(cookiesSetString, cookieExpected, @"Unexpected cookie");

  // make a copy of the fetched data to compare with our next fetch from the
  // cache
  NSData *originalFetchedData = [[fetchedData_ copy] autorelease];
  
  // Now fetch again so the "If modified since" header will be set (because
  // we're calling setFetchHistory: below) and caching ON, and verify that we
  // got a good data from the cache, along with a "Not modified" status

  [self resetFetchResponse];
  
  [self doFetchWithURLString:urlString cachingDatedData:YES];
  
  STAssertEqualObjects(fetchedData_, originalFetchedData, 
                       @"cache data mismatch");
  
  STAssertNotNil(fetchedData_,
                 @"failed to fetch data, status:%ld error:%@, URL:%@",
                 (long)fetchedStatus_, fetcherError_, urlString);  
  STAssertNotNil(fetchedResponse_,
                 @"failed to get fetch response, status:%;d error:%@",
                 (long)fetchedStatus_, fetcherError_);  
  STAssertNotNil(fetchedRequest_, @"failed to get fetch request, URL %@", 
                 urlString);  
  STAssertNil(fetcherError_, @"fetching data gave error: %@", fetcherError_);  
  
  STAssertEquals(fetchedStatus_, (NSInteger)kGTMHTTPFetcherStatusNotModified, // 304
                 @"fetching data expected status 304, instead got %ld, for URL %@",
                 (long)fetchedStatus_, urlString);
  
  // the TestCookie set previously should be sent with this request
  cookiesSent = [[fetchedRequest_ allHTTPHeaderFields] objectForKey:@"Cookie"];
  STAssertEqualObjects(cookiesSent, cookieExpected, @"Cookie not sent");
  
  // Now fetch twice without caching enabled, and verify that we got a 
  // "Not modified" status, along with a non-nil but empty NSData (which 
  // is normal for that status code)
  
  [self resetFetchResponse];
  
  [fetchHistory_ removeAllObjects];
  
  [self doFetchWithURLString:urlString cachingDatedData:NO];

  STAssertEqualObjects(fetchedData_, originalFetchedData, 
                       @"cache data mismatch");

  [self resetFetchResponse];
  [self doFetchWithURLString:urlString cachingDatedData:NO];

  STAssertNotNil(fetchedData_, @"");  
  STAssertEquals([fetchedData_ length], (NSUInteger)0, @"unexpected data");
  STAssertEquals(fetchedStatus_, (NSInteger)kGTMHTTPFetcherStatusNotModified, 
                 @"fetching data expected status 304, instead got %d", fetchedStatus_);
  STAssertNil(fetcherError_, @"unexpected error: %@", fetcherError_); 
 
}

- (void)testBogusFetch {
  // fetch a live, invalid URL
  NSString *badURLString = @"http://localhost:86/";
  [self doFetchWithURLString:badURLString cachingDatedData:NO];
  
  const int kServiceUnavailableStatus = 503;
  
  if (fetchedStatus_ == kServiceUnavailableStatus) {
    // some proxies give a "service unavailable" error for bogus fetches
  } else {
  
    if (fetchedData_) {
      NSString *str = [[[NSString alloc] initWithData:fetchedData_
                                             encoding:NSUTF8StringEncoding] autorelease];
      STAssertNil(fetchedData_, @"fetched unexpected data: %@", str);  
    }
      
    STAssertNotNil(fetcherError_, @"failed to receive fetching error");
    STAssertEquals(fetchedStatus_, (NSInteger)0,
                   @"fetching data expected no status from no response, instead got %d",
                   fetchedStatus_);
  }
   
  // fetch with a specific status code from our http server
  [self resetFetchResponse];

  NSString *invalidWebPageFile =
    [kValidFileName stringByAppendingString:@"?status=400"];
  NSString *statusUrlString =
    [self fileURLStringToTestFileName:invalidWebPageFile];

  [self doFetchWithURLString:statusUrlString cachingDatedData:NO];
    
  STAssertNotNil(fetchedData_, @"fetch lacked data with error info");
  STAssertNil(fetcherError_, @"expected bad status but got an error");
  STAssertEquals(fetchedStatus_, (NSInteger)400,
                 @"unexpected status, error=%@", fetcherError_);
}

- (void)testRetryFetches {
  
  GTMHTTPFetcher *fetcher;
  
  NSString *invalidFile = [kValidFileName stringByAppendingString:@"?status=503"];
  NSString *urlString = [self fileURLStringToTestFileName:invalidFile];
  
  SEL countRetriesSel = @selector(countRetriesFetcher:willRetry:forError:);
  SEL fixRequestSel = @selector(fixRequestFetcher:willRetry:forError:);

  //
  // test: retry until timeout, then expect failure with status message
  //
  
  NSNumber *lotsOfRetriesNumber = [NSNumber numberWithInt:1000];
  
  fetcher= [self doFetchWithURLString:urlString 
                     cachingDatedData:NO
                        retrySelector:countRetriesSel
                     maxRetryInterval:5.0 // retry intervals of 1, 2, 4
                             userData:lotsOfRetriesNumber];
  
  STAssertNotNil(fetchedData_, @"error data is expected");
  STAssertEquals(fetchedStatus_, (NSInteger)503, nil);
  STAssertEquals([fetcher retryCount], 3U, @"retry count unexpected");
  
  //
  // test:  retry twice, then give up
  //
  [self resetFetchResponse];
  
  NSNumber *twoRetriesNumber = [NSNumber numberWithInt:2];
  
  fetcher= [self doFetchWithURLString:urlString 
                     cachingDatedData:NO
                        retrySelector:countRetriesSel
                     maxRetryInterval:10.0 // retry intervals of 1, 2, 4, 8
                             userData:twoRetriesNumber];
  
  STAssertNotNil(fetchedData_, @"error data is expected");
  STAssertEquals(fetchedStatus_, (NSInteger)503, nil); 
  STAssertEquals([fetcher retryCount], 2U, @"retry count unexpected");
  
  
  //
  // test:  retry, making the request succeed on the first retry
  //        by fixing the URL
  //
  [self resetFetchResponse];
  
  fetcher= [self doFetchWithURLString:urlString 
                     cachingDatedData:NO
                        retrySelector:fixRequestSel
                     maxRetryInterval:30.0 // should only retry once due to selector
                             userData:lotsOfRetriesNumber];
  
  STAssertNotNil(fetchedData_, @"data is expected");
  STAssertEquals(fetchedStatus_, (NSInteger)200, nil);
  STAssertEquals([fetcher retryCount], 1U, @"retry count unexpected");
}

#pragma mark -

- (GTMHTTPFetcher *)doFetchWithURLString:(NSString *)urlString 
                        cachingDatedData:(BOOL)doCaching {
  
  return [self doFetchWithURLString:(NSString *)urlString 
                   cachingDatedData:(BOOL)doCaching 
                      retrySelector:nil
                   maxRetryInterval:0
                           userData:nil];
}

- (GTMHTTPFetcher *)doFetchWithURLString:(NSString *)urlString 
                        cachingDatedData:(BOOL)doCaching 
                           retrySelector:(SEL)retrySel
                        maxRetryInterval:(NSTimeInterval)maxRetryInterval
                                userData:(id)userData {
    
  NSURL *url = [NSURL URLWithString:urlString];
  NSURLRequest *req = [NSURLRequest requestWithURL:url
                                       cachePolicy:NSURLRequestReloadIgnoringCacheData
                                   timeoutInterval:kGiveUpInterval];
  GTMHTTPFetcher *fetcher = [GTMHTTPFetcher httpFetcherWithRequest:req];
  
  STAssertNotNil(fetcher, @"Failed to allocate fetcher");
  
  // setting the fetch history will add the "If-modified-since" header
  // to repeat requests
  [fetcher setFetchHistory:fetchHistory_];
  if (doCaching != [fetcher shouldCacheDatedData]) {
    // only set the value when it changes since setting it to nil clears out
    // some of the state and our tests need the state between some non caching
    // fetches.
    [fetcher setShouldCacheDatedData:doCaching];
  }
  
  if (retrySel) {
    [fetcher setIsRetryEnabled:YES];
    [fetcher setRetrySelector:retrySel];
    [fetcher setMaxRetryInterval:maxRetryInterval];
    [fetcher setUserData:userData];
    
    // we force a minimum retry interval for unit testing; otherwise,
    // we'd have no idea how many retries will occur before the max
    // retry interval occurs, since the minimum would be random
    [fetcher setMinRetryInterval:1.0];
  }
  
  BOOL isFetching =
    [fetcher beginFetchWithDelegate:self
                  didFinishSelector:@selector(testFetcher:finishedWithData:)
                    didFailSelector:@selector(testFetcher:failedWithError:)];
  STAssertTrue(isFetching, @"Begin fetch failed");
  
  if (isFetching) {
    
    // Give time for the fetch to happen, but give up if 10 seconds elapse with
    // no response
    NSDate* giveUpDate = [NSDate dateWithTimeIntervalSinceNow:kGiveUpInterval];
    while ((!fetchedData_ && !fetcherError_) &&
           [giveUpDate timeIntervalSinceNow] > 0) {
      NSDate* loopIntervalDate =
        [NSDate dateWithTimeIntervalSinceNow:kRunLoopInterval];
      [[NSRunLoop currentRunLoop] runUntilDate:loopIntervalDate]; 
    }
  }
  
  return fetcher;
}

- (NSString *)fileURLStringToTestFileName:(NSString *)name {
  
  // we need to create http URLs referring to the desired
  // resource to be found by the python http server running locally
  
  // return a localhost:port URL for the test file
  NSString *urlString = [NSString stringWithFormat:@"http://localhost:%d/%@",
    [testServer_ port], name];
  
  // we exclude the "?status=" that would indicate that the URL
  // should cause a retryable error
  NSRange range = [name rangeOfString:@"?status="];
  if (range.length > 0) {
    name = [name substringToIndex:range.location]; 
  }
  
  // we exclude the ".auth" extension that would indicate that the URL
  // should be tested with authentication
  if ([[name pathExtension] isEqual:@"auth"]) {
    name = [name stringByDeletingPathExtension]; 
  }
  
  // just for sanity, let's make sure we see the file locally, so
  // we can expect the Python http server to find it too
  NSBundle *testBundle = [NSBundle bundleForClass:[self class]];
  STAssertNotNil(testBundle, nil);
  
  NSString *filePath =
    [testBundle pathForResource:[name stringByDeletingPathExtension]
                         ofType:[name pathExtension]];
  STAssertNotNil(filePath, nil);
  
  return urlString;  
}



- (void)testFetcher:(GTMHTTPFetcher *)fetcher finishedWithData:(NSData *)data {
  fetchedData_ = [data copy];
  fetchedStatus_ = [fetcher statusCode]; // this implicitly tests that the fetcher has kept the response
  fetchedRequest_ = [[fetcher request] retain];
  fetchedResponse_ = [[fetcher response] retain];
}

- (void)testFetcher:(GTMHTTPFetcher *)fetcher failedWithError:(NSError *)error {
  // if it's a status error, don't hang onto the error, just the status/data
  if ([[error domain] isEqual:kGTMHTTPFetcherStatusDomain]) {
    fetchedData_ = [[[error userInfo] objectForKey:kGTMHTTPFetcherStatusDataKey] copy];
    fetchedStatus_ = [error code]; // this implicitly tests that the fetcher has kept the response
  } else {
    fetcherError_ = [error retain];  
    fetchedStatus_ = [fetcher statusCode];
  }
}


// Selector for allowing up to N retries, where N is an NSNumber in the
// fetcher's userData
- (BOOL)countRetriesFetcher:(GTMHTTPFetcher *)fetcher
                  willRetry:(BOOL)suggestedWillRetry
                   forError:(NSError *)error {
  
  int count = [fetcher retryCount];
  int allowedRetryCount = [[fetcher userData] intValue];
  
  BOOL shouldRetry = (count < allowedRetryCount);
  
  STAssertEquals([fetcher nextRetryInterval], pow(2.0, [fetcher retryCount]),
                 @"unexpected next retry interval (expected %f, was %f)",
                 pow(2.0, [fetcher retryCount]),
                 [fetcher nextRetryInterval]);
  
  return shouldRetry;
}

// Selector for retrying and changing the request to one that will succeed
- (BOOL)fixRequestFetcher:(GTMHTTPFetcher *)fetcher
                willRetry:(BOOL)suggestedWillRetry
                 forError:(NSError *)error {
  
  STAssertEquals([fetcher nextRetryInterval], pow(2.0, [fetcher retryCount]),
                 @"unexpected next retry interval (expected %f, was %f)",
                 pow(2.0, [fetcher retryCount]),
                 [fetcher nextRetryInterval]);
  
  // fix it - change the request to a URL which does not have a status value
  NSString *urlString = [self fileURLStringToTestFileName:kValidFileName]; 
  
  NSURL *url = [NSURL URLWithString:urlString];
  [fetcher setRequest:[NSURLRequest requestWithURL:url]];
  
  return YES; // do the retry fetch; it should succeed now
}

@end
