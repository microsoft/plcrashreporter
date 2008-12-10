/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>

/**
 * @defgroup functions Crash Reporter Functions Reference
 */

/**
 * @defgroup constants Crash Reporter Constants Reference
 */

/**
 * @defgroup plcrash_internal Crash Reporter Internal Documentation
 */

/**
 * @defgroup enums Enumerations
 * @ingroup constants
 */

/**
 * @defgroup globals Global Variables
 * @ingroup constants
 */

/**
 * @defgroup exceptions Exceptions
 * @ingroup constants
 */

/* Exceptions */
extern NSString *PLCrashReporterException;

/* Error Domain and Codes */
extern NSString *PLCrashReporterErrorDomain;

/**
 * NSError codes in the Plausible Crash Reporter error domain.
 * @ingroup enums
 */
typedef enum {
    /** An unknown error has occured. If this
     * code is received, it is a bug, and should be reported. */
    PLCrashReporterErrorUnknown = 0,

    /** An Mach or POSIX operating system error has occured. The underlying cause may be fetched from the userInfo
     * dictionary using the NSUnderlyingErrorKey key. */
    PLCrashReporterErrorOperatingSystem = 1,
} PLCrashReporterError;
