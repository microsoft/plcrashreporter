/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>

/* Dependencies */
#import <Foundation/Foundation.h>

#ifdef __APPLE__
#import <AvailabilityMacros.h>
#endif

#import "PLCrashReporter.h"

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

/**
 * @mainpage Plausible Crash Reporter
 *
 * @section intro_sec Introduction
 *
 * Library classes supporting subclassing are explicitly documented. Due to =
 * Objective-C's fragile base classes, binary compatibility with subclasses is
 * NOT guaranteed. You should avoid subclassing library classes -- use class composition instead.
 *
 *
 * @section doc_sections Documentation Sections
 * - @subpage error_handling
 *
 */

/**
 * @page error_handling Error Handling Programming Guide
 *
 * Where a method may return an error, Plausible Crash Reporter provides access to the underlying
 * cause via an optional NSError argument.
 *
 * All returned errors will be a member of one of the below defined domains, however, new domains and
 * error codes may be added at any time. If you do not wish to report on the error cause, many methods
 * support a simple form that requires no NSError argument.
 *
 * @section Error Domains, Codes, and User Info
 *
 * @subsection crashreporter_errors Crash Reporter Errors
 *
 * Any errors in Plausible Crash Reporter use the #PLCrashReporterErrorDomain error domain, and and one
 * of the error codes defined in #PLCrashReporterError.
 */
