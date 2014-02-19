/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2014 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#import "GTMSenTestCase.h"

#import "PLCrashAsyncFile.hpp"
#include <sys/stat.h>

using namespace plcrash::async;

@interface PLCrashAsyncFile : SenTestCase {
@private
    /* Path to test output file */
    NSString *_outputFile;
    
    /* Open output file descriptor */
    int _testFd;
}

@end

/**
 * Tests the async_file implementation.
 */
@implementation PLCrashAsyncFile

- (void) setUp {
    /* Create a temporary output file */
    _outputFile = [[NSTemporaryDirectory() stringByAppendingString: [[NSProcessInfo processInfo] globallyUniqueString]] retain];
    
    _testFd = open([_outputFile UTF8String], O_RDWR|O_CREAT|O_EXCL, 0644);
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

/**
 * Verify that output is limited to the specified output_limit.
 */
- (void) testWriteLimits {
    uint32_t data = 1;
    
    /* Initialize the file instance with an 8 byte limit */
    async_file file = async_file(_testFd, 8);
    
    /* Write to the limit */
    STAssertTrue(file.write(&data, sizeof(data)), @"Write failed");
    STAssertTrue(file.write(&data, sizeof(data)), @"Write failed");
    STAssertFalse(file.write(&data, sizeof(data)), @"Limit not enforced");
    
    /* Close */
    file.flush();
    file.close();
    
    /* Test the actual file size */
    struct stat fs;
    stat([_outputFile UTF8String], &fs);
    STAssertEquals((off_t)8, fs.st_size, @"File size is not 8 bytes");
}

/**
 * Verify that output is unlimited when output_limit is 0.
 */
- (void) testWriteUnlimited {
    uint32_t data = 1;
    
    /* Initialize the file instance with an no byte limit */
    async_file file = async_file(_testFd, 0);
    
    /* Write 4 bytes */
    STAssertTrue(file.write(&data, sizeof(data)), @"Write failed");
    
    /* Close */
    file.flush();
    file.close();
    
    /* Test the actual file size */
    struct stat fs;
    stat([_outputFile UTF8String], &fs);
    STAssertEquals((off_t)sizeof(data), fs.st_size, @"File size is not 4 bytes");
}

/*
 * Read in the test file, verify that it matches the given data block. Returns the
 * total number of bytes read (which may be less than the data block, which will
 * not trigger an error).
 */
- (size_t) checkTestData: (uint8_t *) data bytes: (size_t) len inputStream: (NSInputStream *) input {
    size_t bytesRead = 0;
    while ([input hasBytesAvailable] && bytesRead < len) {
        unsigned char buf[256];
        size_t nread;
        
        nread = [input read: buf maxLength: len-bytesRead];
        STAssertTrue(memcmp(buf, data + bytesRead, nread) == 0, @"Data does not compare at %zu (just read %zu)", bytesRead, nread);
        bytesRead += nread;
    }
    
    return bytesRead;
}

/**
 * Excercise the interior write buffering.
 */
- (void) testBufferedWrite {
    int write_iterations = 8;
    unsigned char data[100];
    size_t nread = 0;

    /* Initialize the file instance */
    async_file file = async_file(_testFd, 0);
    
    /* Assert that the tests will trigger internal buffering */
    STAssertTrue(sizeof(data) * write_iterations > file.buffer_size(), @"Test is invalid if our test data is larger than the output buffer");
    
    /* Assert that the buffered data will not fall on the buffer boundary; this tests the buffering behavior
     * when a write() would exceed the internal buffer capacity. */
    STAssertFalse(sizeof(data) % file.buffer_size() == 0, @"Test is invalid if our test data falls on the output buffer boundary");

    /* Create test data */
    for (unsigned char i = 0; i < sizeof(data); i++)
        data[i] = i;

    /* Write out the test data */
    for (int i = 0; i < write_iterations; i++)
        STAssertTrue(file.write(data, sizeof(data)), @"Failed to write to output buffer");
    
    /* Flush pending data and close the file */
    STAssertTrue(file.flush(), @"File flush failed");
    STAssertTrue(file.close(), @"File not closed");
    
    /* Validate the test file */
    NSInputStream *input = [NSInputStream inputStreamWithFileAtPath: _outputFile];
    [input open];
    STAssertEquals((NSStreamStatus)NSStreamStatusOpen, [input streamStatus], @"Could not open input stream %@: %@", _outputFile, [input streamError]);
    
    for (int i = 0; i < write_iterations; i++)
        nread += [self checkTestData: data bytes: sizeof(data) inputStream: input];
    
    STAssertEquals(nread, sizeof(data) * write_iterations, @"Fewer than expected bytes were written (%zu < %zu)", nread, sizeof(data) * write_iterations);
    
    [input close];
}

@end
