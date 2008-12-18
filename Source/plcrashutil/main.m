/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
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

/*
 * Run a conversion.
 */
int convert_command (int argc, char *argv[]) {
    const char *format = "iphone";
    const char *input_file;
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
    PLCrashLog *crashLog = [[PLCrashLog alloc] initWithData: data error: &error];
    if (crashLog == nil) {
        fprintf(stderr, "Could not decode crash log: %s\n", [[error localizedDescription] UTF8String]);
        return 1;
    }

    /* Header */

    /* Map to Apple-style code type */
    const char *codeType;
    switch (crashLog.systemInfo.architecture) {
        case PLCrashLogArchitectureARM:
            codeType = "ARM";
            break;
        case PLCrashLogArchitectureX86_32:
            codeType = "X86";
            break;
        case PLCrashLogArchitectureX86_64:
            codeType = "X86-64";
            break;
        default:
            fprintf(stderr, "Unknown architecture type %d", crashLog.systemInfo.architecture);
            return 1;
    }

    fprintf(output, "Incident Identifier: [TODO]\n");
    fprintf(output, "CrashReporter Key:   [TODO]\n");
    fprintf(output, "Process:         [TODO]\n");
    fprintf(output, "Path:            [TODO]\n");
    fprintf(output, "Identifier:      %s\n", [crashLog.applicationInfo.applicationIdentifier UTF8String]);
    fprintf(output, "Version:         %s\n", [crashLog.applicationInfo.applicationVersion UTF8String]);
    fprintf(output, "Code Type:       %s\n", codeType);
    fprintf(output, "Parent Process:  [TODO]\n");

    fprintf(output, "\n");

    /* System info */
    fprintf(output, "Date/Time:       %s\n", [[crashLog.systemInfo.timestamp description] UTF8String]);
    fprintf(output, "OS Version:      [TODO]\n");
    fprintf(output, "Report Version:  103\n");
    
    fprintf(output, "\n");

    /* Exception code */
    fprintf(output, "Exception Type:  [TODO]\n");
    fprintf(output, "Exception Codes: [TODO]\n");
    fprintf(output, "Crashed Thread:  [TODO]\n");

    fprintf(output, "\n");

    /* Threads */
    for (NSUInteger i = 0; i < [crashLog.threads count]; i++) {
        PLCrashLogThreadInfo *thread = [crashLog.threads objectAtIndex: i];

        fprintf(output, "Thread %d:\n", i);
        for (NSUInteger frame_idx = 0; frame_idx < [thread.stackFrames count]; frame_idx++) {
            PLCrashLogStackFrameInfo *frameInfo = [thread.stackFrames objectAtIndex: frame_idx];
            PLCrashLogBinaryImageInfo *imageInfo;

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
    
            fprintf(output, "%d %s\t\t\t0x%" PRIx64 " 0x%" PRIx64 " + 0x%" PRIx64 "\n", 
                    frame_idx, imageName, frameInfo.instructionPointer, baseAddress, pcOffset);
        }
        fprintf(output, "\n");
    }

    /* Images */
    fprintf(output, "Binary Images:\n");
    for (PLCrashLogBinaryImageInfo *imageInfo in crashLog.images) {
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