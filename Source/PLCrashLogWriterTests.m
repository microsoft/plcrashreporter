/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"

#import "PLCrashLogWriter.h"
#import "PLCrashFrameWalker.h"

@interface PLCrashLogWriterTests : SenTestCase {
@private
    /* Path to crash log */
    NSString *_logPath;
    
    /* Writer instance */
    plcrash_writer_t _writer;
    
    /* Test thread */
    plframe_test_thead_t _thr_args;
}

@end


@implementation PLCrashLogWriterTests

- (void) setUp {
    /* Init the writer */
    _logPath = [NSTemporaryDirectory() stringByAppendingString: [[NSProcessInfo processInfo] globallyUniqueString]];
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_writer_init(&_writer, [_logPath UTF8String]), @"Initialization failed");
    
    /* Create the test thread */
    plframe_test_thread_spawn(&_thr_args);
}

- (void) tearDown {
    NSError *error;
    
    /* Close the writer and delete the file */
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_writer_close(&_writer), @"Close failed");
    STAssertTrue([[NSFileManager defaultManager] removeItemAtPath: _logPath error: &error], @"Could not remove log file");

    /* Stop the test thread */
    plframe_test_thread_stop(&_thr_args);
}

- (void) testInit {
    STAssertGreaterThanOrEqual(_writer.fd, 0, @"File descriptor was not opened");
}

- (void) testWriteReport {
    siginfo_t info;
    plframe_cursor_t cursor;
    
    /* Initialze a faux-siginfo instance */
    info.si_addr = 0x0;
    info.si_errno = 0;
    info.si_pid = getpid();
    info.si_uid = getuid();
    info.si_code = SEGV_MAPERR;
    info.si_signo = SIGSEGV;
    info.si_status = 0;
    
    /* Steal the test thread's stack for iteration */
    plframe_cursor_thread_init(&cursor, pthread_mach_thread_np(_thr_args.thread));

    plcrash_writer_report(&_writer, &info, cursor.uap);
}

@end
