/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"
#import "PLCrashAsync.h"


@interface PLCrashAsyncTests : SenTestCase {
@private
    /* Path to test output file */
    NSString *_outputFile;

    /* Open output file descriptor */
    int _testFd;
}

@end


@implementation PLCrashAsyncTests

- (void) setUp {
    /* Create a temporary output file */
    _outputFile = [[NSTemporaryDirectory() stringByAppendingString: [[NSProcessInfo processInfo] globallyUniqueString]] retain];

    _testFd = open([_outputFile UTF8String], O_RDWR|O_CREAT|O_EXCL);
    STAssertTrue(_testFd >= 0, @"Could not open test output file");
}

- (void) tearDown {
    NSError *error;

    /* Close the file (it may already be closed) */
    close(_testFd);

    /* Delete the file */
    STAssertTrue([[NSFileManager defaultManager] removeItemAtPath: _outputFile error: &error], @"Could not remove log file");
    [_outputFile release];
}

/*
 * Read in the test file, verify that it matches the given data block. Returns the
 * total number of bytes read (which may be less than the data block, which will
 * not trigger an error).
 */
- (size_t) checkTestData: (void *) data bytes: (size_t) len inputStream: (NSInputStream *) input {
    size_t bytesRead = 0;
    while ([input hasBytesAvailable] && bytesRead < len) {
        unsigned char buf[256];
        size_t nread;
        
        nread = [input read: buf maxLength: len-bytesRead];
        STAssertTrue(memcmp(buf, data + bytesRead, nread) == 0, @"Data does not compare at %d (just read %d)", bytesRead, nread);
        bytesRead += nread;
    }

    return bytesRead;
}

- (void) testBufferedWrite {
    plasync_file_t file;
    int write_iterations = 8;
    unsigned char data[100];
    size_t nread = 0;
    
    STAssertTrue(sizeof(data) * write_iterations > sizeof(file.buffer), @"Test is invalid if our buffer is not larger");

    /* Initialize the file instance */
    plasync_file_init(&file, _testFd);

    /* Create test data */
    for (unsigned char i = 0; i < sizeof(data); i++)
        data[i] = i;

    /* Write out the test data */
    for (int i = 0; i < write_iterations; i++)
        STAssertTrue(plasync_file_write(&file, data, sizeof(data)), @"Failed to write to output buffer");
    
    /* Flush pending data and close the file */
    STAssertTrue(plasync_file_flush(&file), @"File flush failed");
    STAssertTrue(plasync_file_close(&file), @"File not closed");
    
    /* Validate the test file */
    NSInputStream *input = [NSInputStream inputStreamWithFileAtPath: _outputFile];
    [input open];
    STAssertEquals((NSStreamStatus)NSStreamStatusOpen, [input streamStatus], @"Could not open input stream %@: %@", _outputFile, [input streamError]);
    
    for (int i = 0; i < write_iterations; i++)
        nread += [self checkTestData: data bytes: sizeof(data) inputStream: input];

    STAssertEquals(nread, sizeof(data) * write_iterations, @"Fewer than expected bytes were written (%d < %d)", nread, sizeof(data) * write_iterations);
    
    [input close];
}

@end