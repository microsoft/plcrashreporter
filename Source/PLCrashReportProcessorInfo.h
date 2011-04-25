/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2008-2011 Plausible Labs Cooperative, Inc.
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

/**
 * @ingroup constants
 *
 * Known CPU types.
 *
 * @internal
 * These enum values match the protobuf values. Keep them synchronized.
 */
typedef enum {
    /** Unknown CPU type. */
    PLCrashReportCPUTypeUnkown = 0,

    /** x86-32. */
    PLCrashReportCPUTypeX86_32 = 1,
    
    /** x86-64 */
    PLCrashReportCPUTypeX86_64 = 2,
    
    /** ARM */
    PLCrashReportCPUTypeARM = 3,
    
    /** PPC */
    PLCrashReportCPUTypePPC = 4
} PLCrashReportCPUType;

/**
 * @ingroup constants
 *
 * Known CPU subtypes.
 *
 * @internal
 * These enum values match the protobuf values. Keep them synchronized.
 */
typedef enum {
    /** Unknown CPU subtype. */
    PLCrashReportCPUSubtypeUnkwown = 0,
        
    /* ARMv6 */
    PLCrashReportCPUSubtypeARMv6 = 100,

    /* ARMv7 */
    PLCrashReportCPUSubtypeARMv7 = 101,
} PLCrashReportCPUSubtype;


@interface PLCrashReportProcessorInfo : NSObject {
@private
    /** CPU type */
    PLCrashReportCPUType _type;

    /** CPU subtype */
    PLCrashReportCPUSubtype _subtype;
}

- (id) initWithType: (PLCrashReportCPUType) type subtype: (PLCrashReportCPUSubtype) subtype;

/** The CPU type. */
@property(nonatomic, readonly) PLCrashReportCPUType type;

/** The CPU subtype. */
@property(nonatomic, readonly) PLCrashReportCPUSubtype subtype;

@end
