/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2009 Plausible Labs Cooperative, Inc.
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
#import "PLCrashAsync.h"

#import <fcntl.h>
#import <sys/stat.h>

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

- (void) testMemcpy {
    size_t size = 1024;
    uint8_t template[size];
    uint8_t src[size+1];
    uint8_t dest[size+1];

    /* Create our template. We don't use the template as the source, as it's possible the memcpy implementation
     * could modify src in error, while validation could still succeed if src == dest. */
    memset_pattern4(template, (const uint8_t[]){ 0xC, 0xA, 0xF, 0xE }, size);
    memcpy(src, template, size);

    /* Add mismatched sentinals to the destination and src; serves as a simple check for overrun on write. */
    src[1024] = 0xD;
    dest[1024] = 0xB;

    plcrash_async_memcpy(dest, src, size);

    STAssertTrue(memcmp(template, dest, size) == 0, @"The copied destination does not match the source");
    STAssertTrue(dest[1024] == (uint8_t)0xB, @"Sentinal was overwritten (0x%" PRIX8 ")", dest[1024]);
}

- (void) testWriteLimits {
    plcrash_async_file_t file;
    uint32_t data = 1;

    /* Initialize the file instance with an 8 byte limit */
    plcrash_async_file_init(&file, _testFd, 8);

    /* Write to the limit */
    STAssertTrue(plcrash_async_file_write(&file, &data, sizeof(data)), @"Write failed");
    STAssertTrue(plcrash_async_file_write(&file, &data, sizeof(data)), @"Write failed");
    STAssertFalse(plcrash_async_file_write(&file, &data, sizeof(data)), @"Limit not enforced");

    /* Close */
    plcrash_async_file_flush(&file);
    plcrash_async_file_close(&file);

    /* Test the actual file size */
    struct stat fs;
    stat([_outputFile UTF8String], &fs);
    STAssertEquals((off_t)8, fs.st_size, @"File size is not 8 bytes");
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
    plcrash_async_file_t file;
    int write_iterations = 8;
    unsigned char data[100];
    size_t nread = 0;
    
    STAssertTrue(sizeof(data) * write_iterations > sizeof(file.buffer), @"Test is invalid if our buffer is not larger");

    /* Initialize the file instance */
    plcrash_async_file_init(&file, _testFd, 0);

    /* Create test data */
    for (unsigned char i = 0; i < sizeof(data); i++)
        data[i] = i;

    /* Write out the test data */
    for (int i = 0; i < write_iterations; i++)
        STAssertTrue(plcrash_async_file_write(&file, data, sizeof(data)), @"Failed to write to output buffer");
    
    /* Flush pending data and close the file */
    STAssertTrue(plcrash_async_file_flush(&file), @"File flush failed");
    STAssertTrue(plcrash_async_file_close(&file), @"File not closed");
    
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