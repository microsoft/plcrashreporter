/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>
/** 
 * @ingroup constants
 * Crash file magic identifier */
#define PLCRASH_LOG_FILE_MAGIC "plcrash"

/** 
 * @ingroup constants
 * Crash format version byte identifier. Will not change outside of the introduction of
 * an entirely new crash log format. */
#define PLCRASH_LOG_FILE_VERSION 1

/**
 * @ingroup types
 * Plausible Crash Log Header.
 */
struct PLCrashLogFileHeader {
    /** File magic, not NULL terminated */
    const char magic[7];

    /** File version */
    const uint8_t version;

    /** File data */
    const uint8_t data[];
} __attribute__((packed));


/**
 * @internal
 * Private decoder instance variables (used to hide the underlying protobuf parser).
 */
typedef struct _PLCrashLogDecoder _PLCrashLogDecoder;

@interface PLCrashLog : NSObject {
@private
    /** Private implementation variables (used to hide the underlying protobuf parser) */
    _PLCrashLogDecoder *_decoder;
}

- (id) initWithData: (NSData *) encodedData error: (NSError **) outError;

@end
