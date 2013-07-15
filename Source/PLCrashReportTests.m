/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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
#import "PLCrashReport.h"
#import "PLCrashReporter.h"
#import "PLCrashFrameWalker.h"
#import "PLCrashLogWriter.h"
#import "PLCrashAsyncImageList.h"

#import <fcntl.h>
#import <dlfcn.h>

#import <objc/runtime.h>

#import <mach-o/arch.h>
#import <mach-o/dyld.h>

@interface PLCrashReportTests : SenTestCase {
@private
    /* Path to crash log */
    NSString *_logPath;
}

@end

@implementation PLCrashReportTests

- (void) setUp {
    /* Create a temporary log path */
    _logPath = [[NSTemporaryDirectory() stringByAppendingString: [[NSProcessInfo processInfo] globallyUniqueString]] retain];
}

- (void) tearDown {
    NSError *error;
    
    /* Delete the file */
    STAssertTrue([[NSFileManager defaultManager] removeItemAtPath: _logPath error: &error], @"Could not remove log file");
    [_logPath release];
}

- (void) testWriteReport {
    siginfo_t info;
    plcrash_log_writer_t writer;
    plcrash_async_file_t file;
    plcrash_async_image_list_t image_list;
    NSError *error = nil;
    
    /* Initialze faux crash data */
    {
        info.si_addr = method_getImplementation(class_getInstanceMethod([self class], _cmd));
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
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_log_writer_init(&writer, @"test.id", @"1.0", false), @"Initialization failed");
    
    /* Set an exception with a valid return address call stack. */
    NSException *exception;
    @try {
        [NSException raise: @"TestException" format: @"TestReason"];
    }
    @catch (NSException *e) {
        exception = e;
    }
    plcrash_log_writer_set_exception(&writer, exception);

    /* Provide binary image info */
    plcrash_nasync_image_list_init(&image_list, mach_task_self());
    uint32_t image_count = _dyld_image_count();
    for (uint32_t i = 0; i < image_count; i++) {
        plcrash_nasync_image_list_append(&image_list, (uintptr_t) _dyld_get_image_header(i), _dyld_get_image_vmaddr_slide(i), _dyld_get_image_name(i));
    }

    /* Write the crash report */    
    STAssertEquals(PLCRASH_ESUCCESS, plcrash_log_writer_write_curthread(&writer, &image_list, &file, &info), @"Writing crash log failed");

    /* Close it */
    plcrash_log_writer_close(&writer);
    plcrash_log_writer_free(&writer);
    plcrash_nasync_image_list_free(&image_list);

    plcrash_async_file_flush(&file);
    plcrash_async_file_close(&file);

    /* Try to parse it */
    PLCrashReport *crashLog = [[[PLCrashReport alloc] initWithData: [NSData dataWithContentsOfMappedFile: _logPath] error: &error] autorelease];
    STAssertNotNil(crashLog, @"Could not decode crash log: %@", error);

    /* Report info */
    STAssertNotNULL(crashLog.uuidRef, @"No report UUID");
    
    /* System info */
    STAssertNotNil(crashLog.systemInfo, @"No system information available");
    STAssertNotNil(crashLog.systemInfo.operatingSystemVersion, @"OS version is nil");
    STAssertNotNil(crashLog.systemInfo.operatingSystemBuild, @"OS build is nil");
    STAssertNotNil(crashLog.systemInfo.timestamp, @"Timestamp is nil");
    STAssertEquals(crashLog.systemInfo.operatingSystem, PLCrashReportHostOperatingSystem, @"Operating system incorrect");
    STAssertEquals(crashLog.systemInfo.architecture, PLCrashReportHostArchitecture, @"Architecture incorrect");
    
    /* Machine info */
    const NXArchInfo *archInfo = NXGetLocalArchInfo();
    STAssertTrue(crashLog.hasMachineInfo, @"No machine information available");
    STAssertNotNil(crashLog.machineInfo, @"No machine information available");
    STAssertNotNil(crashLog.machineInfo.modelName, @"Model is nil");
    STAssertEquals(PLCrashReportProcessorTypeEncodingMach, crashLog.machineInfo.processorInfo.typeEncoding, @"Incorrect processor type encoding");
    STAssertEquals((uint64_t)archInfo->cputype, crashLog.machineInfo.processorInfo.type, @"Incorrect processor type");
    STAssertEquals((uint64_t)archInfo->cpusubtype, crashLog.machineInfo.processorInfo.subtype, @"Incorrect processor subtype");
    STAssertNotEquals((NSUInteger)0, crashLog.machineInfo.processorCount, @"No processor count");
    STAssertNotEquals((NSUInteger)0, crashLog.machineInfo.logicalProcessorCount, @"No logical processor count");

    /* App info */
    STAssertNotNil(crashLog.applicationInfo, @"No application information available");
    STAssertNotNil(crashLog.applicationInfo.applicationIdentifier, @"No application identifier available");
    STAssertNotNil(crashLog.applicationInfo.applicationVersion, @"No application version available");
    
    /* Process info */
    STAssertNotNil(crashLog.processInfo, @"No process information available");
    STAssertNotNil(crashLog.processInfo.processName, @"No process name available");
    STAssertNotNil(crashLog.processInfo.processPath, @"No process path available");
    STAssertNotNil(crashLog.processInfo.parentProcessName, @"No parent process name available");
    STAssertTrue(crashLog.processInfo.native, @"Process should be native");
    
    /* Signal info */
    STAssertEqualStrings(@"SIGSEGV", crashLog.signalInfo.name, @"Signal is incorrect");
    STAssertEqualStrings(@"SEGV_MAPERR", crashLog.signalInfo.code, @"Signal code is incorrect");
    
    /* Exception info */
    STAssertNotNil(crashLog.exceptionInfo, @"Exception info is nil");
    STAssertEqualStrings(crashLog.exceptionInfo.exceptionName, [exception name], @"Exceptio name is incorrect");
    STAssertEqualStrings(crashLog.exceptionInfo.exceptionReason, [exception reason], @"Exception name is incorrect");
    NSUInteger exceptionFrameCount = [[exception callStackReturnAddresses] count];
    for (NSUInteger i = 0; i < exceptionFrameCount; i++) {
        NSNumber *retAddr = [[exception callStackReturnAddresses] objectAtIndex: i];
        PLCrashReportStackFrameInfo *sf = [crashLog.exceptionInfo.stackFrames objectAtIndex: i];
        STAssertEquals(sf.instructionPointer, [retAddr unsignedLongLongValue], @"Stack frame address is incorrect");
    }

    /* Thread info */
    STAssertNotNil(crashLog.threads, @"Thread list is nil");
    STAssertNotEquals((NSUInteger)0, [crashLog.threads count], @"No thread values returned");

    NSUInteger thrNumber = 0;
    NSInteger lastThreadNumber;
    BOOL crashedFound = NO;
    for (PLCrashReportThreadInfo *threadInfo in crashLog.threads) {
        STAssertNotNil(threadInfo.stackFrames, @"Thread stackframe list is nil");
        STAssertNotNil(threadInfo.registers, @"Thread register list is nil");
        if (thrNumber > 0) {
            STAssertTrue(lastThreadNumber < threadInfo.threadNumber, @"Threads are listed out of order.");
        }
        lastThreadNumber = threadInfo.threadNumber;

        if (threadInfo.crashed) {
            STAssertNotEquals((NSUInteger)0, [threadInfo.registers count], @"No registers recorded for the crashed thread");
            for (PLCrashReportRegisterInfo *registerInfo in threadInfo.registers) {
                STAssertNotNil(registerInfo.registerName, @"Register name is nil");
            }

            /* Symbol information should be available for an ObjC frame in our binary */
            STAssertNotEquals((NSUInteger)0, [threadInfo.stackFrames count], @"Zero stack frames returned");
            PLCrashReportStackFrameInfo *stackFrame = [threadInfo.stackFrames objectAtIndex: 0];
            STAssertNotNil(stackFrame.symbolInfo, @"No symbol info found");
            
            NSString *symName = [NSString stringWithFormat: @"-[%@ %@]", [self class], NSStringFromSelector(_cmd)];
            STAssertEqualStrings(stackFrame.symbolInfo.symbolName, symName, @"Incorrect symbol name");

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
        
        STAssertNotNil(imageInfo.codeType, @"Image code type is nil");
        STAssertEquals(imageInfo.codeType.typeEncoding, PLCrashReportProcessorTypeEncodingMach, @"Incorrect type encoding");
        
        /*
         * Find the in-memory mach header for the image record. We'll compare this against the serialized data.
         *
         * The 32-bit and 64-bit mach_header structures are equivalent for our purposes.
         *
         * The (uint64_t)(uint32_t) casting is prevent improper sign extension when casting the signed cpusubtype integer_t
         * to a larger, unsigned uint64_t value.
         */
        Dl_info info;
        STAssertTrue(dladdr((void *)(uintptr_t)imageInfo.imageBaseAddress, &info) != 0, @"dladdr() failed to find image");
        struct mach_header *hdr = info.dli_fbase;
        STAssertEquals(imageInfo.codeType.type, (uint64_t)(uint32_t)hdr->cputype, @"Incorrect CPU type");
        STAssertEquals(imageInfo.codeType.subtype, (uint64_t)(uint32_t)hdr->cpusubtype, @"Incorrect CPU subtype");
    }
}


@end
