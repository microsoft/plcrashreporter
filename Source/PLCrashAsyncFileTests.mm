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

#include <pthread.h>
#include <sys/stat.h>
#include <sys/socket.h>
#import <objc/runtime.h>

using namespace plcrash::async;

@interface PLCrashAsyncFile : SenTestCase {
@private
    /* Path to test output directory; this will be deleted on test completion */
    NSString *_outputDir;

    /* Path to test output file */
    NSString *_outputFile;
    
    /* Open output file descriptor */
    int _testFd;
}

@end

/**
 * Tests the AsyncFile implementation.
 */
@implementation PLCrashAsyncFile

- (void) setUp {
    NSError *error;
    
    /* Create a temporary directory to hold all test files */
    NSString *uniqueString = [[[NSProcessInfo processInfo] globallyUniqueString] stringByAppendingFormat: @"%s", class_getName([self class])];
    _outputDir = [[NSTemporaryDirectory() stringByAppendingPathComponent: uniqueString] retain];
    STAssertTrue([[NSFileManager defaultManager] createDirectoryAtPath: _outputDir withIntermediateDirectories: YES attributes: @{ NSFilePosixPermissions: @(0755) } error: &error], @"Could not create output directory: %@", _outputDir);

    /* Create a temporary output file */
    _outputFile = [[_outputDir stringByAppendingPathComponent: [[NSProcessInfo processInfo] globallyUniqueString]] retain];
    _testFd = open([_outputFile UTF8String], O_RDWR|O_CREAT|O_EXCL, 0644);
    STAssertTrue(_testFd >= 0, @"Could not open test output file");
}

- (void) tearDown {
    NSError *error;
    
    /* Close the file (it may already be closed) */
    close(_testFd);
    
    /* Delete all test data */
    STAssertTrue([[NSFileManager defaultManager] removeItemAtPath: _outputDir error: &error], @"Could not remove output directory: %@", error);

    [_outputDir release];
    [_outputFile release];
}

/**
 * Test temporary file creation.
 */
- (void) testMkTemp {
    /* Generate a template string */
    char *ptemplate;
    
    /* Size of the template string buffer, including NUL */
    size_t ptemplate_size;
    
    /* Size of the overflow-check buffer containing the template string */
    size_t ptemplate_overflow_check_size;

    {
        /* Generate the base template string */
        char *ptemplate_base = strdup([[_outputDir stringByAppendingPathComponent: @"mktemp_test_XXX_foo.XXXXXX"] fileSystemRepresentation]);
        ptemplate_size = strlen(ptemplate_base) + 1;
        
        /* Set up a larger buffer string that we'll use to detect any off-by-one overflow errors
         * in the template implementation */
        ptemplate_overflow_check_size = ptemplate_size + 10; /* + overflow check buffer */
        ptemplate = (char *) malloc(ptemplate_overflow_check_size);
        memset(ptemplate, 'B', ptemplate_overflow_check_size);
        strcpy(ptemplate, ptemplate_base);

        /* Clean up the initial template buffer */
        free(ptemplate_base);
    }

    /* Try creating the temporary file */
    int tempFD;
    STAssertEquals(PLCRASH_ESUCCESS, AsyncFile::mktemp(ptemplate, 0600, &tempFD), @"mktemp() returned an error");
    STAssertTrue(tempFD >= 0, @"Failed to create output file descriptor");
    close(tempFD);

    NSString *tempOutputFile = [[[NSString alloc] initWithUTF8String: ptemplate] retain];
    STAssertNotNil(tempOutputFile, @"String initialization failed for '%s'", ptemplate);

    /* Verify file open+creation */
    STAssertTrue([[NSFileManager defaultManager] fileExistsAtPath: tempOutputFile], @"File was not created at the expected path: %@", tempOutputFile);
    
    /* Verify that the template was updated */
    STAssertTrue([[tempOutputFile lastPathComponent] hasPrefix: @"mktemp_test_XXX_foo."], @"The temporary file's prefix was modified: %@", [tempOutputFile lastPathComponent]);
    STAssertFalse([[tempOutputFile lastPathComponent] hasSuffix: @"XXXXXX"], @"The temporary file's suffix was not modified: %@", [tempOutputFile lastPathComponent]);
    
    /* Verify that no overflow occured */
    STAssertTrue(ptemplate[ptemplate_size-1] == '\0', @"Missing trailing NUL");
    for (size_t i = ptemplate_size; i < ptemplate_overflow_check_size; i++) {
        STAssertTrue(ptemplate[i] == 'B', @"Safety padding was overwritten");
    }

    /* Clean up our template allocations */
    free(ptemplate);
}

/**
 * Verify that mktemp() succeeds with an empty suffix value.
 */
- (void) testMkTempNoSuffix {
    /* Generate a template string with no trailing X's. */
    NSString *fileName = [[[NSProcessInfo processInfo] globallyUniqueString] stringByAppendingString: @".no_trailing"];
    NSString *filePath = [_outputDir stringByAppendingPathComponent: fileName];
    char *ptemplate = strdup([filePath fileSystemRepresentation]);
    
    /* Verify that mktemp() succeds when no suffix is specified. ... */
    int tempFD;
    STAssertEquals(PLCRASH_ESUCCESS, AsyncFile::mktemp(ptemplate, 0600, &tempFD), @"mktemp() returned an error");
    close(tempFD);
    
    /* Verify that the path was not modified */
    STAssertEqualStrings(filePath, [NSString stringWithUTF8String: ptemplate], @"Template was modified despite not containing X-suffix");
    
    /* Clean up our template allocation */
    free(ptemplate);
}

/**
 * Test temporary file handling; verify that:
 * - We attempt all possible combinations.
 * - An error is returned if all possible combinations exist on disk.
 */
- (void) testMkTempCombinations {
    /* Generate a template string. */
    NSString *uuid = [[[[NSUUID alloc] init] autorelease] UUIDString];
    NSString *fileName = [uuid stringByAppendingString: @"-mktemp_combo_test.XX"];
    NSString *filePath = [_outputDir stringByAppendingPathComponent: fileName];
    
    /* Create all possible combinations of the temporary file name. This relies on internal knowledge of the number of padding characters
     * used in mktemp(); the test will fail if the padding character count changes. */
    int possibleValues = 1;
    for (int i = 0; i < 2; i++) {
        possibleValues = possibleValues * 36;
    }
    
    for (int i = 0; i < possibleValues; i++) {
        char *ptemplate = strdup([filePath fileSystemRepresentation]);
        
        int tempFD;
        plcrash_error_t err = AsyncFile::mktemp(ptemplate, 0600, &tempFD);
        free(ptemplate);

        STAssertEquals(PLCRASH_ESUCCESS, err, @"mktemp() returned an error at position %d", i);
        if (err == PLCRASH_ESUCCESS) {
            close(tempFD);
        } else {
            break;
        }
    }

    /* Now that all possible paths exist, the next request should fail. */
    char *ptemplate = strdup([filePath fileSystemRepresentation]);
    int tempFD;
    STAssertEquals(PLCRASH_OUTPUT_ERR, AsyncFile::mktemp(ptemplate, 0600, &tempFD), @"mktemp() did not return an error");
    free(ptemplate);
}

/* Writer thread used for testReadWriteN */
struct background_writer_ctx {
    int wfd;
    const void *data;
    size_t data_len;
    ssize_t written;
};

static void *background_writer_thread (void *arg) {
    struct background_writer_ctx *ctx = (struct background_writer_ctx *) arg;
    ctx->written = AsyncFile::writen(ctx->wfd, ctx->data, ctx->data_len);
    return NULL;
}

/**
 * Verify that writen() and readn() work as advertised. Unfortunately, there's no obvious way to unit tests
 * partial read/write handling; SO_SNDBUF/SO_RCVBUF are no-ops on Mac OS X AF_UNIX sockets, and since our
 * reads and writes are blocking, we can't watch for EAGAIN and use it to signal the reader/writer
 * on the other end of the pipe to start/stop reading.
 *
 * Fortunately, writen() and readn() are short and easily reviewed, and it's -very- obvious if they fail to
 * operate as expected.
 */
- (void) testReadWriteN {
    const uint8_t test_data[] = { 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x12 };

    /* Set up a socket pair for testing. */
    int rfd, wfd;
    {
        int fds[2];
        STAssertEquals(0, socketpair(PF_UNIX, SOCK_STREAM, 0, fds), @"Failed to create socket pair");
        rfd = fds[0];
        wfd = fds[1];
    }
    
    /* Set up a background writer; we need to execute writing asynchronously, given that both reading and writing
     * will block. */
    pthread_t writer_thr;
    struct background_writer_ctx ctx = {
        .wfd = wfd,
        .data = test_data,
        .data_len = sizeof(test_data),
        .written = 0
    };
    STAssertEquals(0, pthread_create(&writer_thr, NULL, background_writer_thread, &ctx), @"Failed to start writer thread");
    
    /* Perform the read */
    uint8_t recv_data[sizeof(test_data)];
    STAssertEquals((ssize_t)sizeof(recv_data), AsyncFile::readn(rfd, recv_data, sizeof(recv_data)), @"Read failed");
    
    pthread_join(writer_thr, NULL);
    STAssertEquals((ssize_t)sizeof(test_data), ctx.written, @"Write failed!");
    
    /* Verify the result */
    STAssertEquals(memcmp(test_data, recv_data, sizeof(test_data)), 0, @"Received data does not match test data");

    /* Clean up */
    close(rfd);
    close(wfd);
}

/**
 * Verify that output is limited to the specified output_limit.
 */
- (void) testWriteLimits {
    uint32_t data = 1;
    
    /* Initialize the file instance with an 8 byte limit */
    AsyncFile file = AsyncFile(_testFd, 8);
    
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
    AsyncFile file = AsyncFile(_testFd, 0);
    
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
    AsyncFile file = AsyncFile(_testFd, 0);
    
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
