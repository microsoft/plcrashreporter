//
//  GTMNSData+zlibTest.m
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
#import "GTMUnitTestDevLog.h"
#import "GTMNSData+zlib.h"
#import <stdlib.h> // for randiom/srandomdev
#import <zlib.h>

@interface GTMNSData_zlibTest : GTMTestCase
@end

  
static void FillWithRandom(char *data, unsigned long len) {
  char *max = data + len;
  for ( ; data < max ; ++data) {
    *data = random() & 0xFF;
  }
}

static BOOL HasGzipHeader(NSData *data) {
  // very simple check
  const unsigned char *bytes = [data bytes];
  return ([data length] > 2) &&
         ((bytes[0] == 0x1f) && (bytes[1] == 0x8b));
}


@implementation GTMNSData_zlibTest

- (void)setUp {
  // seed random from /dev/random
  srandomdev();
}

- (void)testBoundryValues {
  NSAutoreleasePool *localPool = [[NSAutoreleasePool alloc] init];
  STAssertNotNil(localPool, @"failed to alloc local pool");
  
  // build some test data
  NSMutableData *data = [NSMutableData data];
  STAssertNotNil(data, @"failed to alloc data block");
  [data setLength:512];
  FillWithRandom([data mutableBytes], [data length]);

  // bogus args to start
  STAssertNil([NSData gtm_dataByDeflatingData:nil], nil);
  STAssertNil([NSData gtm_dataByDeflatingBytes:nil length:666], nil);
  STAssertNil([NSData gtm_dataByDeflatingBytes:[data bytes] length:0], nil);
  STAssertNil([NSData gtm_dataByGzippingData:nil], nil);
  STAssertNil([NSData gtm_dataByGzippingBytes:nil length:666], nil);
  STAssertNil([NSData gtm_dataByGzippingBytes:[data bytes] length:0], nil);
  STAssertNil([NSData gtm_dataByInflatingData:nil], nil);
  STAssertNil([NSData gtm_dataByInflatingBytes:nil length:666], nil);
  STAssertNil([NSData gtm_dataByInflatingBytes:[data bytes] length:0], nil);
  
  // test deflate w/ compression levels out of range
  NSData *deflated = [NSData gtm_dataByDeflatingData:data
                                    compressionLevel:-4];
  STAssertNotNil(deflated, nil);
  STAssertFalse(HasGzipHeader(deflated), nil);
  NSData *dataPrime = [NSData gtm_dataByInflatingData:deflated];
  STAssertNotNil(dataPrime, nil);
  STAssertEqualObjects(data, dataPrime, nil);
  deflated = [NSData gtm_dataByDeflatingData:data
                             compressionLevel:20];
  STAssertNotNil(deflated, nil);
  STAssertFalse(HasGzipHeader(deflated), nil);
  dataPrime = [NSData gtm_dataByInflatingData:deflated];
  STAssertNotNil(dataPrime, nil);
  STAssertEqualObjects(data, dataPrime, nil);

  // test gzip w/ compression levels out of range
  NSData *gzipped = [NSData gtm_dataByGzippingData:data
                                   compressionLevel:-4];
  STAssertNotNil(gzipped, nil);
  STAssertTrue(HasGzipHeader(gzipped), nil);
  dataPrime = [NSData gtm_dataByInflatingData:gzipped];
  STAssertNotNil(dataPrime, nil);
  STAssertEqualObjects(data, dataPrime, nil);
  gzipped = [NSData gtm_dataByGzippingData:data
                           compressionLevel:20];
  STAssertNotNil(gzipped, nil);
  STAssertTrue(HasGzipHeader(gzipped), nil);
  dataPrime = [NSData gtm_dataByInflatingData:gzipped];
  STAssertNotNil(dataPrime, nil);
  STAssertEqualObjects(data, dataPrime, nil);
  
  // test non-compressed data data itself
  [GTMUnitTestDevLog expectString:@"Error trying to inflate some of the "
   "payload, error -3"];
  STAssertNil([NSData gtm_dataByInflatingData:data], nil);
  
  // test deflated data runs that end before they are done
  [GTMUnitTestDevLog expect:[deflated length] - 1
              casesOfString:@"Error trying to inflate some of the payload, "
   "error -5"];
  for (NSUInteger x = 1 ; x < [deflated length] ; ++x) {
    STAssertNil([NSData gtm_dataByInflatingBytes:[deflated bytes] 
                                          length:x], nil);
  }
  
  // test gzipped data runs that end before they are done
  [GTMUnitTestDevLog expect:[gzipped length] - 1
              casesOfString:@"Error trying to inflate some of the payload, "
   "error -5"];
  for (NSUInteger x = 1 ; x < [gzipped length] ; ++x) {
    STAssertNil([NSData gtm_dataByInflatingBytes:[gzipped bytes] 
                                          length:x], nil);
  }
  
  // test extra data before the deflated/gzipped data (just to make sure we
  // don't seek to the "real" data)
  NSMutableData *prefixedDeflated = [NSMutableData data];
  STAssertNotNil(prefixedDeflated, @"failed to alloc data block");
  [prefixedDeflated setLength:20];
  FillWithRandom([prefixedDeflated mutableBytes], [prefixedDeflated length]);
  [prefixedDeflated appendData:deflated];
  [GTMUnitTestDevLog expectString:@"Error trying to inflate some of the "
   "payload, error -3"];
  STAssertNil([NSData gtm_dataByInflatingData:prefixedDeflated], nil);
  [GTMUnitTestDevLog expectString:@"Error trying to inflate some of the "
   "payload, error -3"];
  STAssertNil([NSData gtm_dataByInflatingBytes:[prefixedDeflated bytes]
                                        length:[prefixedDeflated length]],
              nil);
  NSMutableData *prefixedGzipped = [NSMutableData data];
  STAssertNotNil(prefixedDeflated, @"failed to alloc data block");
  [prefixedGzipped setLength:20];
  FillWithRandom([prefixedGzipped mutableBytes], [prefixedGzipped length]);
  [prefixedGzipped appendData:gzipped];
  [GTMUnitTestDevLog expectString:@"Error trying to inflate some of the "
   "payload, error -3"];
  STAssertNil([NSData gtm_dataByInflatingData:prefixedGzipped], nil);
  [GTMUnitTestDevLog expectString:@"Error trying to inflate some of the "
   "payload, error -3"];
  STAssertNil([NSData gtm_dataByInflatingBytes:[prefixedGzipped bytes]
                                        length:[prefixedGzipped length]],
              nil);

  // test extra data after the deflated/gzipped data (just to make sure we
  // don't ignore some of the data)
  NSMutableData *suffixedDeflated = [NSMutableData data];
  STAssertNotNil(suffixedDeflated, @"failed to alloc data block");
  [suffixedDeflated appendData:deflated];
  [suffixedDeflated appendBytes:[data bytes] length:20];
  [GTMUnitTestDevLog expectString:@"thought we finished inflate w/o using "
   "all input, 20 bytes left"];
  STAssertNil([NSData gtm_dataByInflatingData:suffixedDeflated], nil);
  [GTMUnitTestDevLog expectString:@"thought we finished inflate w/o using "
   "all input, 20 bytes left"];
  STAssertNil([NSData gtm_dataByInflatingBytes:[suffixedDeflated bytes]
                                        length:[suffixedDeflated length]],
              nil);
  NSMutableData *suffixedGZipped = [NSMutableData data];
  STAssertNotNil(suffixedGZipped, @"failed to alloc data block");
  [suffixedGZipped appendData:gzipped];
  [suffixedGZipped appendBytes:[data bytes] length:20];
  [GTMUnitTestDevLog expectString:@"thought we finished inflate w/o using "
   "all input, 20 bytes left"];
  STAssertNil([NSData gtm_dataByInflatingData:suffixedGZipped], nil);
  [GTMUnitTestDevLog expectString:@"thought we finished inflate w/o using "
   "all input, 20 bytes left"];
  STAssertNil([NSData gtm_dataByInflatingBytes:[suffixedGZipped bytes]
                                        length:[suffixedGZipped length]],
              nil);
  
  [localPool release];
}

- (void)testInflateDeflate {
  // generate a range of sizes w/ random content
  for (int n = 0 ; n < 2 ; ++n) {
    for (int x = 1 ; x < 128 ; ++x) {
      NSAutoreleasePool *localPool = [[NSAutoreleasePool alloc] init];
      STAssertNotNil(localPool, @"failed to alloc local pool");

      NSMutableData *data = [NSMutableData data];
      STAssertNotNil(data, @"failed to alloc data block");

      // first pass small blocks, second pass, larger ones, but second pass
      // avoid making them multimples of 128.
      [data setLength:((n*x*128) + x)];
      FillWithRandom([data mutableBytes], [data length]);

      // w/ *Bytes apis, default level
      NSData *deflated = [NSData gtm_dataByDeflatingBytes:[data bytes] 
                                                   length:[data length]];
      STAssertNotNil(deflated, @"failed to deflate data block");
      STAssertGreaterThan([deflated length], 
                          (NSUInteger)0, @"failed to deflate data block");
      STAssertFalse(HasGzipHeader(deflated), @"has gzip header on zlib data");
      NSData *dataPrime = [NSData gtm_dataByInflatingBytes:[deflated bytes] 
                                                    length:[deflated length]];
      STAssertNotNil(dataPrime, @"failed to inflate data block");
      STAssertGreaterThan([dataPrime length], 
                          (NSUInteger)0, @"failed to inflate data block");
      STAssertEqualObjects(data, 
                           dataPrime, @"failed to round trip via *Bytes apis");

      // w/ *Data apis, default level
      deflated = [NSData gtm_dataByDeflatingData:data];
      STAssertNotNil(deflated, @"failed to deflate data block");
      STAssertGreaterThan([deflated length], 
                          (NSUInteger)0, @"failed to deflate data block");
      STAssertFalse(HasGzipHeader(deflated), @"has gzip header on zlib data");
      dataPrime = [NSData gtm_dataByInflatingData:deflated];
      STAssertNotNil(dataPrime, @"failed to inflate data block");
      STAssertGreaterThan([dataPrime length], 
                          (NSUInteger)0, @"failed to inflate data block");
      STAssertEqualObjects(data, 
                           dataPrime, @"failed to round trip via *Data apis");

      // loop over the compression levels
      for (int level = 1 ; level < 9 ; ++level) {
        // w/ *Bytes apis, using our level
        deflated = [NSData gtm_dataByDeflatingBytes:[data bytes]
                                             length:[data length]
                                   compressionLevel:level];
        STAssertNotNil(deflated, @"failed to deflate data block");
        STAssertGreaterThan([deflated length], 
                            (NSUInteger)0, @"failed to deflate data block");
        STAssertFalse(HasGzipHeader(deflated), @"has gzip header on zlib data");
        dataPrime = [NSData gtm_dataByInflatingBytes:[deflated bytes] 
                                              length:[deflated length]];
        STAssertNotNil(dataPrime, @"failed to inflate data block");
        STAssertGreaterThan([dataPrime length], 
                            (NSUInteger)0, @"failed to inflate data block");
        STAssertEqualObjects(data, 
                             dataPrime, @"failed to round trip via *Bytes apis");

        // w/ *Data apis, using our level
        deflated = [NSData gtm_dataByDeflatingData:data compressionLevel:level];
        STAssertNotNil(deflated, @"failed to deflate data block");
        STAssertGreaterThan([deflated length], 
                            (NSUInteger)0, @"failed to deflate data block");
        STAssertFalse(HasGzipHeader(deflated), @"has gzip header on zlib data");
        dataPrime = [NSData gtm_dataByInflatingData:deflated];
        STAssertNotNil(dataPrime, @"failed to inflate data block");
        STAssertGreaterThan([dataPrime length], 
                            (NSUInteger)0, @"failed to inflate data block");
        STAssertEqualObjects(data, 
                             dataPrime, @"failed to round trip via *Data apis");
      }

      [localPool release];
    }
  }
}

- (void)testInflateGzip {
  // generate a range of sizes w/ random content
  for (int n = 0 ; n < 2 ; ++n) {
    for (int x = 1 ; x < 128 ; ++x) {
      NSAutoreleasePool *localPool = [[NSAutoreleasePool alloc] init];
      STAssertNotNil(localPool, @"failed to alloc local pool");

      NSMutableData *data = [NSMutableData data];
      STAssertNotNil(data, @"failed to alloc data block");

      // first pass small blocks, second pass, larger ones, but second pass
      // avoid making them multimples of 128.
      [data setLength:((n*x*128) + x)];
      FillWithRandom([data mutableBytes], [data length]);

      // w/ *Bytes apis, default level
      NSData *gzipped = [NSData gtm_dataByGzippingBytes:[data bytes] 
                                                 length:[data length]];
      STAssertNotNil(gzipped, @"failed to gzip data block");
      STAssertGreaterThan([gzipped length], 
                          (NSUInteger)0, @"failed to gzip data block");
      STAssertTrue(HasGzipHeader(gzipped), 
                   @"doesn't have gzip header on gzipped data");
      NSData *dataPrime = [NSData gtm_dataByInflatingBytes:[gzipped bytes] 
                                                    length:[gzipped length]];
      STAssertNotNil(dataPrime, @"failed to inflate data block");
      STAssertGreaterThan([dataPrime length], 
                          (NSUInteger)0, @"failed to inflate data block");
      STAssertEqualObjects(data, 
                           dataPrime, @"failed to round trip via *Bytes apis");

      // w/ *Data apis, default level
      gzipped = [NSData gtm_dataByGzippingData:data];
      STAssertNotNil(gzipped, @"failed to gzip data block");
      STAssertGreaterThan([gzipped length], 
                          (NSUInteger)0, @"failed to gzip data block");
      STAssertTrue(HasGzipHeader(gzipped), 
                   @"doesn't have gzip header on gzipped data");
      dataPrime = [NSData gtm_dataByInflatingData:gzipped];
      STAssertNotNil(dataPrime, @"failed to inflate data block");
      STAssertGreaterThan([dataPrime length], 
                          (NSUInteger)0, @"failed to inflate data block");
      STAssertEqualObjects(data, dataPrime, 
                           @"failed to round trip via *Data apis");

      // loop over the compression levels
      for (int level = 1 ; level < 9 ; ++level) {
        // w/ *Bytes apis, using our level
        gzipped = [NSData gtm_dataByGzippingBytes:[data bytes]
                                           length:[data length]
                                 compressionLevel:level];
        STAssertNotNil(gzipped, @"failed to gzip data block");
        STAssertGreaterThan([gzipped length], 
                            (NSUInteger)0, @"failed to gzip data block");
        STAssertTrue(HasGzipHeader(gzipped), 
                     @"doesn't have gzip header on gzipped data");
        dataPrime = [NSData gtm_dataByInflatingBytes:[gzipped bytes] 
                                              length:[gzipped length]];
        STAssertNotNil(dataPrime, @"failed to inflate data block");
        STAssertGreaterThan([dataPrime length], 
                            (NSUInteger)0, @"failed to inflate data block");
        STAssertEqualObjects(data, dataPrime, 
                             @"failed to round trip via *Bytes apis");

        // w/ *Data apis, using our level
        gzipped = [NSData gtm_dataByGzippingData:data compressionLevel:level];
        STAssertNotNil(gzipped, @"failed to gzip data block");
        STAssertGreaterThan([gzipped length], 
                            (NSUInteger)0, @"failed to gzip data block");
        STAssertTrue(HasGzipHeader(gzipped), 
                     @"doesn't have gzip header on gzipped data");
        dataPrime = [NSData gtm_dataByInflatingData:gzipped];
        STAssertNotNil(dataPrime, @"failed to inflate data block");
        STAssertGreaterThan([dataPrime length], 
                            (NSUInteger)0, @"failed to inflate data block");
        STAssertEqualObjects(data, 
                             dataPrime, @"failed to round trip via *Data apis");
      }

      [localPool release];
    }
  }
}

@end
