/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"

#import "PLCrashLogWriter.h"

@interface PLCrashLogWriterTests : SenTestCase {
@private
    /* Path to crash log */
    NSString *_logPath;
    
    /* Writer instance */
    plcrash_writer_t _writer;
}

@end


@implementation PLCrashLogWriterTests

- (void) setUp {
    /* Init the writer */
    _logPath = [NSTemporaryDirectory() stringByAppendingString: [[NSProcessInfo processInfo] globallyUniqueString]];
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_writer_init(&_writer, [_logPath UTF8String]), @"Initialization failed");
}

- (void) tearDown {
    NSError *error;
    
    /* Close the writer and delete the file */
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_writer_close(&_writer), @"Close failed");
    STAssertTrue([[NSFileManager defaultManager] removeItemAtPath: _logPath error: &error], @"Could not remove log file");
}

- (void) testInit {
    STAssertGreaterThanOrEqual(_writer.fd, 0, @"File descriptor was not opened");
    
}

@end
