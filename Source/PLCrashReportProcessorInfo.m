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

#import "PLCrashReportProcessorInfo.h"

/**
 * Crash log processor record.
 *
 * This contains information about a specific processor type and subtype, and may be used
 * to differentiate between processor variants (eg, ARMv6 vs ARMv7).
 */
@implementation PLCrashReportProcessorInfo

@synthesize type = _type;
@synthesize subtype = _subtype;

/**
 * Initialize the processor info data object.
 *
 * @param type The CPU type.
 * @param subtype The CPU subtype
 */
- (id) initWithType: (PLCrashReportCPUType) type subtype: (PLCrashReportCPUSubtype) subtype {
    if ((self = [super init]) == nil)
        return nil;
    
    _type = type;
    _subtype = subtype;

    return self;
}

@end
