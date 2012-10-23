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

#import "PLCrashLogWriter.h"
#import "PLCrashFrameWalker.h"
#import "PLCrashAsyncImageList.h"
#import "PLCrashReport.h"

#import <sys/stat.h>
#import <sys/mman.h>
#import <fcntl.h>
#import <dlfcn.h>

#import <mach-o/loader.h>
#import <mach-o/dyld.h>

#import "crash_report.pb-c.h"

@interface PLCrashLogWriterTests : SenTestCase {
@private
    /* Path to crash log */
    NSString *_logPath;
    
    /* Test thread */
    plframe_test_thead_t _thr_args;
}

@end


@implementation PLCrashLogWriterTests

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

// check a crash report's system info
- (void) checkSystemInfo: (Plcrash__CrashReport *) crashReport {
    Plcrash__CrashReport__SystemInfo *systemInfo = crashReport->system_info;

    STAssertNotNULL(systemInfo, @"No system info available");
    // Nothing else to do?
    if (systemInfo == NULL)
        return;

    STAssertEquals(systemInfo->operating_system, PLCrashReportHostOperatingSystem, @"Unexpected OS value");
    
    STAssertNotNULL(systemInfo->os_version, @"No OS version encoded");

    STAssertEquals(systemInfo->architecture, PLCrashReportHostArchitecture, @"Unexpected machine type");

    STAssertTrue(systemInfo->timestamp != 0, @"Timestamp uninitialized");
}

// check a crash report's app info
- (void) checkAppInfo: (Plcrash__CrashReport *) crashReport {
    Plcrash__CrashReport__ApplicationInfo *appInfo = crashReport->application_info;
    
    STAssertNotNULL(appInfo, @"No app info available");
    // Nothing else to do?
    if (appInfo == NULL)
        return;

    STAssertTrue(strcmp(appInfo->identifier, "test.id") == 0, @"Incorrect app ID written");
    STAssertTrue(strcmp(appInfo->version, "1.0") == 0, @"Incorrect app version written");
}


- (void) checkThreads: (Plcrash__CrashReport *) crashReport {
    Plcrash__CrashReport__Thread **threads = crashReport->threads;
    BOOL foundCrashed = NO;

    STAssertNotNULL(threads, @"No thread messages were written");
    STAssertTrue(crashReport->n_threads > 0, @"0 thread messages were written");

    for (int i = 0; i < crashReport->n_threads; i++) {
        Plcrash__CrashReport__Thread *thread = threads[i];

        /* Check that the threads are provided in order */
        STAssertEquals((uint32_t)i, thread->thread_number, @"Threads were encoded out of order (%d vs %d)", i, thread->thread_number);

        /* Check that there is at least one frame */
        STAssertNotEquals((size_t)0, thread->n_frames, @"No frames available in backtrace");
        
        /* Check for crashed thread */
        if (thread->crashed) {
            foundCrashed = YES;
            STAssertNotEquals((size_t)0, thread->n_registers, @"No registers available on crashed thread");
        }
        
        for (int j = 0; j < thread->n_frames; j++) {
            Plcrash__CrashReport__Thread__StackFrame *f = thread->frames[j];

            /* It is possible for a mach thread to have pc=0 in the first frame. This is the case when a mach thread is
             * first created -- its initial state is 0, and it has a suspend count of 1. */
            if (j > 0)
                STAssertNotEquals((uint64_t)0, f->pc, @"Backtrace includes NULL pc");
        }
    }

    STAssertTrue(foundCrashed, @"No crashed thread was provided");
}

- (void) checkBinaryImages: (Plcrash__CrashReport *) crashReport {
    Plcrash__CrashReport__BinaryImage **images = crashReport->binary_images;

    STAssertNotNULL(images, @"No image messages were written");
    STAssertTrue(crashReport->n_binary_images, @"0 thread messages were written");

    for (int i = 0; i < crashReport->n_binary_images; i++) {
        Plcrash__CrashReport__BinaryImage *image = images[i];
        
        STAssertNotNULL(image->name, @"Null image name");
        STAssertTrue(image->name[0] == '/', @"Image is not absolute path");
        STAssertNotNULL(image->code_type, @"Null code type");
        STAssertEquals(image->code_type->encoding, PLCrashReportProcessorTypeEncodingMach, @"Incorrect type encoding");

        /*
         * Find the in-memory mach header for the image record. We'll compare this against the serialized data.
         *
         * The 32-bit and 64-bit mach_header structures are equivalent for our purposes.
         */ 
        Dl_info info;
        STAssertTrue(dladdr((void *)(uintptr_t)image->base_address, &info) != 0, @"dladdr() failed to find image");
        struct mach_header *hdr = info.dli_fbase;
        STAssertEquals(image->code_type->type, hdr->cputype, @"Incorrect CPU type");
        STAssertEquals(image->code_type->subtype, hdr->cpusubtype, @"Incorrect CPU subtype");
    }
}

- (void) checkException: (Plcrash__CrashReport *) crashReport {
    Plcrash__CrashReport__Exception *exception = crashReport->exception;
    
    STAssertNotNULL(exception, @"No exception was written");
    STAssertTrue(strcmp(exception->name, "TestException") == 0, @"Exception name was not correctly serialized");
    STAssertTrue(strcmp(exception->reason, "TestReason") == 0, @"Exception reason was not correctly serialized");

    STAssertTrue(exception->n_frames, @"0 exception frames were written");
    for (int i = 0; i < exception->n_frames; i++) {
        Plcrash__CrashReport__Thread__StackFrame *f = exception->frames[i];
        STAssertNotEquals((uint64_t)0, f->pc, @"Backtrace includes NULL pc");
    }
}

- (Plcrash__CrashReport *) loadReport {
    /* Reading the report */
    NSData *data = [NSData dataWithContentsOfMappedFile: _logPath];
    STAssertNotNil(data, @"Could not map pages");

    
    /* Check the file magic. The file must be large enough for the value + version + data */
    const struct PLCrashReportFileHeader *header = [data bytes];
    STAssertTrue([data length] > sizeof(struct PLCrashReportFileHeader), @"File is too small for magic + version + data");
    // verifies correct byte ordering of the file magic
    STAssertTrue(memcmp(header->magic, PLCRASH_REPORT_FILE_MAGIC, strlen(PLCRASH_REPORT_FILE_MAGIC)) == 0, @"File header is not 'plcrash', is: '%s'", (const char *) &header->magic);
    STAssertEquals(header->version, (uint8_t) PLCRASH_REPORT_FILE_VERSION, @"File version is not equal to 0");
    
    /* Try to read the crash report */
    Plcrash__CrashReport *crashReport;
    crashReport = plcrash__crash_report__unpack(&protobuf_c_system_allocator, [data length] - sizeof(struct PLCrashReportFileHeader), header->data);
    
    /* If reading the report didn't fail, test the contents */
    STAssertNotNULL(crashReport, @"Could not decode crash report");

    return crashReport;
}


- (void) testWriteReport {
    siginfo_t info;
    plframe_cursor_t cursor;
    plcrash_log_writer_t writer;
    plcrash_async_file_t file;
    plcrash_async_image_list_t image_list;

    /* Initialze faux crash data */
    {
        info.si_addr = (void *) 0x42;
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
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_log_writer_init(&writer, @"test.id", @"1.0", false), @"Initialization failed");
    
    /* Initialize the image list */
    plcrash_async_image_list_init(&image_list, mach_task_self());

    /* Set an exception with a valid return address call stack. */
    NSException *e;
    @try {
        [NSException raise: @"TestException" format: @"TestReason"];
    }
    @catch (NSException *exception) {
        e = exception;
    }
    plcrash_log_writer_set_exception(&writer, e);

    /* Write the crash report */
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_log_writer_write(&writer, &image_list, &file, &info, cursor.uap), @"Crash log failed");

    /* Close it */
    plcrash_log_writer_close(&writer);
    plcrash_log_writer_free(&writer);
    plcrash_async_image_list_free(&image_list);

    /* Flush the output */
    plcrash_async_file_flush(&file);
    plcrash_async_file_close(&file);

    /* Load and validate the written report */
    Plcrash__CrashReport *crashReport = [self loadReport];
    STAssertFalse(crashReport->report_info->user_requested, @"Report not correctly marked as non-user-requested");

    /* Test the report */
    [self checkSystemInfo: crashReport];
    [self checkThreads: crashReport];
    [self checkException: crashReport];
    
    /* Check the signal info */
    STAssertTrue(strcmp(crashReport->signal->name, "SIGSEGV") == 0, @"Signal incorrect");
    STAssertTrue(strcmp(crashReport->signal->code, "SEGV_MAPERR") == 0, @"Signal code incorrect");
    STAssertEquals((uint64_t) 0x42, crashReport->signal->address, @"Signal address incorrect");
    
    /* Free it */
    protobuf_c_message_free_unpacked((ProtobufCMessage *) crashReport, &protobuf_c_system_allocator);
}

static uintptr_t getPC () {
    return (uintptr_t) __builtin_return_address(0);
}

/**
 * Test writing a 'live' report for the current thread.
 */
- (void) testWriteCurrentThreadReport {
    siginfo_t info;
    plcrash_log_writer_t writer;
    plcrash_async_file_t file;
    plcrash_async_image_list_t image_list;
    
    /* Initialze faux crash data */
    {
        info.si_addr = (void *) 0x42;
        info.si_errno = 0;
        info.si_pid = getpid();
        info.si_uid = getuid();
        info.si_code = SEGV_MAPERR;
        info.si_signo = SIGSEGV;
        info.si_status = 0;
    }
    
    /* Open the output file */
    int fd = open([_logPath UTF8String], O_RDWR|O_CREAT|O_EXCL, 0644);
    plcrash_async_file_init(&file, fd, 0);
    
    /* Initialize a writer */
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_log_writer_init(&writer, @"test.id", @"1.0", true), @"Initialization failed");
    
    /* Provide binary image info */
    plcrash_async_image_list_init(&image_list, mach_task_self());
    uint32_t image_count = _dyld_image_count();
    for (uint32_t i = 0; i < image_count; i++) {
        plcrash_async_image_list_append(&image_list, (pl_vm_address_t) _dyld_get_image_header(i), _dyld_get_image_vmaddr_slide(i), _dyld_get_image_name(i));
    }

    
    /* Write the crash report */
    plcrash_error_t ret = plcrash_log_writer_write_curthread(&writer, &image_list, &file, &info);
    uintptr_t expectedPC = getPC();

    STAssertEquals(PLCRASH_ESUCCESS, ret, @"Crash log failed");
    
    /* Close it */
    plcrash_log_writer_close(&writer);
    plcrash_log_writer_free(&writer);
    plcrash_async_image_list_free(&image_list);
    
    /* Flush the output */
    plcrash_async_file_flush(&file);
    plcrash_async_file_close(&file);
    
    
    /* Try to read the crash report */
    NSError *error;
    PLCrashReport *report = [[[PLCrashReport alloc] initWithData: [NSData dataWithContentsOfMappedFile: _logPath] error: &error] autorelease];
    STAssertNotNil(report, @"Failed to read crash report: %@", error);
    
    /* Load and validate the written report */
    Plcrash__CrashReport *crashReport = [self loadReport];
    
    /* Test the report */
    STAssertTrue(crashReport->report_info->user_requested, @"Report not correctly marked as user-requested");
    [self checkSystemInfo: crashReport];
    [self checkThreads: crashReport];

    /* Check the signal info */
    STAssertTrue(strcmp(crashReport->signal->name, "SIGSEGV") == 0, @"Signal incorrect");
    STAssertTrue(strcmp(crashReport->signal->code, "SEGV_MAPERR") == 0, @"Signal code incorrect");
    STAssertEquals((uint64_t) 0x42, crashReport->signal->address, @"Signal address incorrect");
    
    /* Validate register state. We use the more convenient ObjC API for this. */
    BOOL foundCrashed = NO;
    for (int i = 0; i < crashReport->n_threads; i++) {
        Plcrash__CrashReport__Thread *thread = crashReport->threads[i];        
        if (!thread->crashed)
            continue;
        
        foundCrashed = YES;

        /* Load the first frame */
        STAssertNotEquals((size_t)0, thread->n_frames, @"No frames available in backtrace");
        Plcrash__CrashReport__Thread__StackFrame *f = thread->frames[0];

        /* Validate PC. This check is inexact, as otherwise we would need to carefully instrument the 
         * call to plcrash_log_writer_write_curthread() in order to determine the exact PC value. */
        STAssertTrue(expectedPC - f->pc <= 20, @"PC value not within reasonable range");

        /* Extract the registers */
        NSMutableDictionary *regs = [NSMutableDictionary new];
        for (int j = 0; j < thread->n_registers; j++) {
            Plcrash__CrashReport__Thread__RegisterValue *rv = thread->registers[j];
            [regs setObject: [NSNumber numberWithUnsignedLongLong: rv->value] forKey: [NSString stringWithUTF8String: rv->name]];
        }
        
        uintptr_t (^GetReg)(NSString *name) = ^(NSString *name) {
            NSNumber *num = [regs objectForKey: name];
            STAssertNotNil(num, @"Missing requested register %@", name);
            return (uintptr_t) [num unsignedLongLongValue];
        };

        /* Architecture specific validations */
        uint8_t *stackaddr = pthread_get_stackaddr_np(pthread_self());
        size_t stacksize = pthread_get_stacksize_np(pthread_self());

#if __x86_64__
        /* Verify that RSP is sane. */
        uintptr_t rsp = GetReg(@"rsp");
        STAssertTrue((uint8_t *)rsp < stackaddr && (uint8_t *) rsp >= stackaddr-stacksize, @"RSP outside of stack range");

#elif __i386__
        /* Verify that ESP is sane. */
        uintptr_t esp = GetReg(@"esp");
        STAssertTrue((uint8_t *)esp < stackaddr && (uint8_t *)esp >= stackaddr-stacksize, @"ESP outside of stack range");
        
#elif __arm__
        // TODO 
        /* Validate LR */
        void *retaddr = __builtin_return_address(0);
        uintptr_t lr = GetReg(@"lr");
        STAssertEquals(retaddr, (void *)lr, @"Incorrect lr: %p", (void *) lr);

        /* Verify that SP is sane. */
        uintptr_t r7 = GetReg(@"r7");
        STAssertTrue((uint8_t *)r7 < stackaddr && (uint8_t *)r7 >= stackaddr-stacksize, @"SP outside of stack range");
#else
#error Unsupported Platform
#endif
    }
    
    STAssertTrue(foundCrashed, @"No thread marked as crashed");
    
    /* Free it */
    protobuf_c_message_free_unpacked((ProtobufCMessage *) crashReport, &protobuf_c_system_allocator);
}


@end
