/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"
#import "PLCrashLog.h"
#import "PLCrashReporter.h"
#import "PLCrashFrameWalker.h"
#import "PLCrashLogWriter.h"

#import <fcntl.h>

@interface PLCrashLogTests : SenTestCase {
@private
    /* Path to crash log */
    NSString *_logPath;

    /* Test thread */
    plframe_test_thead_t _thr_args;
}

@end

@implementation PLCrashLogTests

- (void) setUp {
    /* Create a temporary log path */
    _logPath = [[NSTemporaryDirectory() stringByAppendingString: [[NSProcessInfo processInfo] globallyUniqueString]] retain];
    
    /* Create the test thread */
    plframe_test_thread_spawn(&_thr_args);
}

- (void) tearDown {
    NSError *error;
    
    /* Delete the file */
    STAssertTrue([[NSFileManager defaultManager] removeItemAtPath: _logPath error: &error], @"Could not remove log file");
    [_logPath release];
    
    /* Stop the test thread */
    plframe_test_thread_stop(&_thr_args);
}

- (void) testWriteReport {
    siginfo_t info;
    plframe_cursor_t cursor;
    plcrash_log_writer_t writer;
    plcrash_async_file_t file;
    NSError *error = nil;
    
    /* Initialze faux crash data */
    {
        info.si_addr = 0x0;
        info.si_errno = 0;
        info.si_pid = getpid();
        info.si_uid = getuid();
        info.si_code = SEGV_MAPERR;
        info.si_signo = SIGSEGV;
        info.si_status = 0;
        
        /* Steal the test thread's stack for iteration */
        plframe_cursor_thread_init(&cursor, pthread_mach_thread_np(_thr_args.thread));
    }
    
    /* Open the output file */
    int fd = open([_logPath UTF8String], O_RDWR|O_CREAT|O_EXCL, 0644);
    plcrash_async_file_init(&file, fd, 0);
    
    /* Initialize a writer */
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_log_writer_init(&writer, @"test.id", @"1.0"), @"Initialization failed");
    
    /* Set an exception */
    plcrash_log_writer_set_exception(&writer, [NSException exceptionWithName: @"TestException" reason: @"TestReason" userInfo: nil]);
    
    /* Write the crash report */
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_log_writer_write(&writer, &file, &info, cursor.uap), @"Crash log failed");
    
    /* Close it */
    plcrash_log_writer_close(&writer);
    plcrash_log_writer_free(&writer);

    plcrash_async_file_flush(&file);
    plcrash_async_file_close(&file);

    /* Try to parse it */
    PLCrashLog *crashLog = [[[PLCrashLog alloc] initWithData: [NSData dataWithContentsOfMappedFile: _logPath] error: &error] autorelease];
    STAssertNotNil(crashLog, @"Could not decode crash log: %@", error);

    STAssertNotNil(crashLog.systemInfo, @"No system information available");
    STAssertNotNil(crashLog.systemInfo.operatingSystemVersion, @"OS version is nil");
    STAssertNotNil(crashLog.systemInfo.timestamp, @"Timestamp is nil");

}


@end
