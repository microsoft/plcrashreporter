/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "GTMSenTestCase.h"
#import "PLCrashReport.h"
#import "PLCrashReporter.h"
#import "PLCrashFrameWalker.h"
#import "PLCrashLogWriter.h"

#import <fcntl.h>

@interface PLCrashReportTests : SenTestCase {
@private
    /* Path to crash log */
    NSString *_logPath;

    /* Test thread */
    plframe_test_thead_t _thr_args;
}

@end

@implementation PLCrashReportTests

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
    PLCrashReport *crashLog = [[[PLCrashReport alloc] initWithData: [NSData dataWithContentsOfMappedFile: _logPath] error: &error] autorelease];
    STAssertNotNil(crashLog, @"Could not decode crash log: %@", error);

    /* System info */
    STAssertNotNil(crashLog.systemInfo, @"No system information available");
    STAssertNotNil(crashLog.systemInfo.operatingSystemVersion, @"OS version is nil");
    STAssertNotNil(crashLog.systemInfo.timestamp, @"Timestamp is nil");
    STAssertEquals(crashLog.systemInfo.operatingSystem, PLCrashReportHostOperatingSystem, @"Operating system incorrect");
    STAssertEquals(crashLog.systemInfo.architecture, PLCrashReportHostArchitecture, @"Architecture incorrect");

    /* App info */
    STAssertNotNil(crashLog.applicationInfo, @"No application information available");
    STAssertNotNil(crashLog.applicationInfo.applicationIdentifier, @"No application identifier available");
    STAssertNotNil(crashLog.applicationInfo.applicationVersion, @"No application version available");

    /* Signal info */
    STAssertEqualStrings(@"SIGSEGV", crashLog.signalInfo.name, @"Signal is incorrect");
    STAssertEqualStrings(@"SEGV_MAPERR", crashLog.signalInfo.code, @"Signal code is incorrect");

    /* Thread info */
    STAssertNotNil(crashLog.threads, @"Thread list is nil");
    STAssertNotEquals((NSUInteger)0, [crashLog.threads count], @"No thread values returned");

    NSUInteger thrNumber = 0;
    BOOL crashedFound = NO;
    for (PLCrashReportThreadInfo *threadInfo in crashLog.threads) {
        STAssertNotNil(threadInfo.stackFrames, @"Thread stackframe list is nil");
        STAssertNotNil(threadInfo.registers, @"Thread register list is nil");
        STAssertEquals((NSInteger)thrNumber, threadInfo.threadNumber, @"Threads are listed out of order.");

        if (threadInfo.crashed) {
            STAssertNotEquals((NSUInteger)0, [threadInfo.registers count], @"No registers recorded for the crashed thread");
            for (PLCrashReportRegisterInfo *registerInfo in threadInfo.registers) {
                STAssertNotNil(registerInfo.registerName, @"Register name is nil");
            }
            crashedFound = YES;
        }
        
        thrNumber++;
    }
    STAssertTrue(crashedFound, @"No crashed thread was found in the crash log");

    /* Image info */
    STAssertNotEquals((NSUInteger)0, [crashLog.images count], @"Crash log should contain at least one image");
    for (PLCrashReportBinaryImageInfo *imageInfo in crashLog.images) {
        STAssertNotNil(imageInfo.imageName, @"Image name is nil");
        if (imageInfo.hasImageUUID == YES) {
            STAssertNotNil(imageInfo.imageUUID, @"Image UUID is nil");
            STAssertEquals((NSUInteger)32, [imageInfo.imageUUID length], @"UUID should be 32 characters (16 bytes)");
        } else if (!imageInfo.hasImageUUID) {
            STAssertNil(imageInfo.imageUUID, @"Info declares no UUID, but the imageUUID property is non-nil");
        }
    }
}


@end
