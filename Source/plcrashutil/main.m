/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2010 Plausible Labs Cooperative, Inc.
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

#import <Foundation/Foundation.h>
#import <CrashReporter/CrashReporter.h>

#import <stdlib.h>
#import <stdio.h>
#include <getopt.h>


/*
 * Print command line usage.
 */
void print_usage () {
    fprintf(stderr, "Usage: plcrashutil <command> <options>\n"
                    "Commands:\n"
                    "  convert --format=<format> <file>\n"
                    "      Covert a plcrash file to the given format.\n\n"
                    "      Supported formats:\n"
                    "        iphone - Standard Apple iPhone-compatible text crash log\n");
}

/**
 * Sort PLCrashReportBinaryImageInfo instances by their starting address.
 */
NSInteger binaryImageSort(id binary1, id binary2, void *context) {
    uint64_t addr1 = [binary1 imageBaseAddress];
    uint64_t addr2 = [binary2 imageBaseAddress];
    
    if (addr1 < addr2)
        return NSOrderedAscending;
    else if (addr1 > addr2)
        return NSOrderedDescending;
    else
        return NSOrderedSame;
}

/*
 * Run a conversion.
 */
int convert_command (int argc, char *argv[]) {
    const char *format = "iphone";
    const char *input_file;
    boolean_t lp64;
    FILE *output = stdout;

    /* options descriptor */
    static struct option longopts[] = {
        { "format",     required_argument,      NULL,          'f' },
        { NULL,         0,                      NULL,           0 }
    };    

    /* Read the options */
    char ch;
    while ((ch = getopt_long(argc, argv, "f:", longopts, NULL)) != -1) {
        switch (ch) {
            case 'f':
                format = optarg;
                break;
            default:
                print_usage();
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    /* Ensure there's an input file specified */
    if (argc < 1) {
        fprintf(stderr, "No input file supplied\n");
        print_usage();
        return 1;
    } else {
        input_file = argv[0];
    }
    
    /* Verify that the format is supported. Only one is actually supported currently */
    if (strcmp(format, "iphone") != 0) {
        fprintf(stderr, "Unsupported format requested\n");
        print_usage();
        return 1;
    }

    /* Try reading the file in */
    NSError *error;
    NSData *data = [NSData dataWithContentsOfFile: [NSString stringWithUTF8String: input_file] 
                                          options: NSMappedRead error: &error];
    if (data == nil) {
        fprintf(stderr, "Could not read input file: %s\n", [[error localizedDescription] UTF8String]);
        return 1;
    }
    
    /* Decode it */
    PLCrashReport *crashLog = [[PLCrashReport alloc] initWithData: data error: &error];
    if (crashLog == nil) {
        fprintf(stderr, "Could not decode crash log: %s\n", [[error localizedDescription] UTF8String]);
        return 1;
    }

    /* Header */

    /* Map to apple style OS nane */
    const char *osName;
    switch (crashLog.systemInfo.operatingSystem) {
        case PLCrashReportOperatingSystemMacOSX:
            osName = "Mac OS X";
            break;
        case PLCrashReportOperatingSystemiPhoneOS:
            osName = "iPhone OS";
            break;
        case PLCrashReportOperatingSystemiPhoneSimulator:
            osName = "Mac OS X";
            break;
        default:
            fprintf(stderr, "Unknown operating system type %d", crashLog.systemInfo.operatingSystem);
            return 1;
    }
    
    /* Map to Apple-style code type, and mark whether architecture is LP64 (64-bit) */
    const char *codeType;
    switch (crashLog.systemInfo.architecture) {
        case PLCrashReportArchitectureARM:
            codeType = "ARM";
            lp64 = false;
            break;
        case PLCrashReportArchitectureX86_32:
            codeType = "X86";
            lp64 = false;
            break;
        case PLCrashReportArchitectureX86_64:
            codeType = "X86-64";
            lp64 = true;
            break;
        case PLCrashReportArchitecturePPC:
            codeType = "PPC";
            lp64 = false;
            break;
        default:
            fprintf(stderr, "Unknown architecture type %d", crashLog.systemInfo.architecture);
            return 1;
    }

    fprintf(output, "Incident Identifier: [TODO]\n");
    fprintf(output, "CrashReporter Key:   [TODO]\n");

    /* Application and process info */
    {
        const char *unknownString = "???";

        const char *processName = unknownString;
        const char *processId = unknownString;
        const char *processPath = unknownString;
        const char *parentProcessName = unknownString;
        const char *parentProcessId = unknownString;

        /* Process information was not available in earlier crash report versions */
        if (crashLog.hasProcessInfo) {
            /* Process Name */
            if (crashLog.processInfo.processName != nil)
                processName = [crashLog.processInfo.processName UTF8String];
            
            /* PID */
            processId = [[[NSNumber numberWithUnsignedInteger: crashLog.processInfo.processID] stringValue] UTF8String];

            /* Process Path */
            if (crashLog.processInfo.processPath != nil)
                processPath = [crashLog.processInfo.processPath UTF8String];

            /* Parent Process Name */
            if (crashLog.processInfo.parentProcessName != nil)
                parentProcessName = [crashLog.processInfo.parentProcessName UTF8String];
    
            /* Parent Process ID */
            parentProcessId = [[[NSNumber numberWithUnsignedInteger: crashLog.processInfo.parentProcessID] stringValue] UTF8String];
        }

        fprintf(output, "Process:         %s [%s]\n", processName, processId);
        fprintf(output, "Path:            %s\n", processPath);
        fprintf(output, "Identifier:      %s\n", [crashLog.applicationInfo.applicationIdentifier UTF8String]);
        fprintf(output, "Version:         %s\n", [crashLog.applicationInfo.applicationVersion UTF8String]);
        fprintf(output, "Code Type:       %s\n", codeType);
        fprintf(output, "Parent Process:  %s [%s]\n", parentProcessName, parentProcessId);
    }

    fprintf(output, "\n");

    /* System info */
    fprintf(output, "Date/Time:       %s\n", [[crashLog.systemInfo.timestamp description] UTF8String]);
    fprintf(output, "OS Version:      %s %s (TODO)\n", osName, [crashLog.systemInfo.operatingSystemVersion UTF8String]);
    fprintf(output, "Report Version:  103\n");
    
    fprintf(output, "\n");

    /* Exception code */
    fprintf(output, "Exception Type:  %s\n", [crashLog.signalInfo.name UTF8String]);
    fprintf(output, "Exception Codes: %s at 0x%" PRIx64 "\n", [crashLog.signalInfo.code UTF8String], crashLog.signalInfo.address);

    for (PLCrashReportThreadInfo *thread in crashLog.threads) {
        if (thread.crashed) {
            fprintf(output, "Crashed Thread:  %ld\n", (long) thread.threadNumber);
            break;
        }
    }

    fprintf(output, "\n");
    
    /* Uncaught Exception */
    if (crashLog.hasExceptionInfo) {
        fprintf(output, "Application Specific Information:\n");
        fprintf(output, "*** Terminating app due to uncaught exception '%s', reason: '%s'\n",
                [crashLog.exceptionInfo.exceptionName UTF8String], [crashLog.exceptionInfo.exceptionReason UTF8String]);

        fprintf(output, "\n");
    }

    /* Threads */
    PLCrashReportThreadInfo *crashed_thread = nil;
    for (PLCrashReportThreadInfo *thread in crashLog.threads) {
        if (thread.crashed) {
            fprintf(output, "Thread %ld Crashed:\n", (long) thread.threadNumber);
            crashed_thread = thread;
        } else {
            fprintf(output, "Thread %ld:\n", (long) thread.threadNumber);
        }
        for (NSUInteger frame_idx = 0; frame_idx < [thread.stackFrames count]; frame_idx++) {
            PLCrashReportStackFrameInfo *frameInfo = [thread.stackFrames objectAtIndex: frame_idx];
            PLCrashReportBinaryImageInfo *imageInfo;

            /* Base image address containing instrumention pointer, offset of the IP from that base
             * address, and the associated image name */
            uint64_t baseAddress = 0x0;
            uint64_t pcOffset = 0x0;
            const char *imageName = "\?\?\?";
            
            imageInfo = [crashLog imageForAddress: frameInfo.instructionPointer];
            if (imageInfo != nil) {
                imageName = [[imageInfo.imageName lastPathComponent] UTF8String];
                baseAddress = imageInfo.imageBaseAddress;
                pcOffset = frameInfo.instructionPointer - imageInfo.imageBaseAddress;
            }
    
            fprintf(output, "%-4ld%-36s0x%08" PRIx64 " 0x%" PRIx64 " + %" PRId64 "\n", 
                    (long) frame_idx, imageName, frameInfo.instructionPointer, baseAddress, pcOffset);
        }
        fprintf(output, "\n");
    }
    
    /* Registers */
    if (crashed_thread != nil) {
        fprintf(output, "Thread %ld crashed with %s Thread State:\n", (long) crashed_thread.threadNumber, codeType);
        
        int regColumn = 1;
        for (PLCrashReportRegisterInfo *reg in crashed_thread.registers) {
            const char *reg_fmt;
            
            /* Use 32-bit or 64-bit fixed width format for the register values */
            if (lp64)
                reg_fmt = "%6s:\t0x%016" PRIx64 " ";
            else
                reg_fmt = "%6s:\t0x%08" PRIx64 " ";

            fprintf(output, reg_fmt, [reg.registerName UTF8String], reg.registerValue);
            
            if (regColumn % 4 == 0)
                fprintf(output, "\n");
            regColumn++;
        }
        
        if (regColumn % 3 != 0)
            fprintf(output, "\n");
            
        fprintf(output, "\n");
    }

    /* Images. The iPhone crash report format sorts these in ascending order, by the base address */
    fprintf(output, "Binary Images:\n");
    for (PLCrashReportBinaryImageInfo *imageInfo in [crashLog.images sortedArrayUsingFunction: binaryImageSort context: nil]) {
        NSString *uuid;
        /* Fetch the UUID if it exists */
        if (imageInfo.hasImageUUID)
            uuid = imageInfo.imageUUID;
        else
            uuid = @"???";
    
        /* base_address - terminating_address file_name identifier (<version>) <uuid> file_path */
        fprintf(output, "0x%" PRIx64 " - 0x%" PRIx64 "  %s \?\?\? (\?\?\?) <%s> %s\n",
            imageInfo.imageBaseAddress,
            imageInfo.imageBaseAddress + imageInfo.imageSize,
            [[imageInfo.imageName lastPathComponent] UTF8String],
            [uuid UTF8String],
            [imageInfo.imageName UTF8String]);
    }

    return 0;
}

int main (int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int ret = 0;

    if (argc < 2) {
        print_usage();
        exit(1);
    }

    /* Convert command */
    if (strcmp(argv[1], "convert") == 0) {
        ret = convert_command(argc - 2, argv + 2);
    } else {
        print_usage();
        ret = 1;
    }

    [pool release];
    exit(ret);
}
