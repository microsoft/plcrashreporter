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

#import "PLCrashReportThreadInfo.h"

/**
 * Crash log per-thread state information.
 *
 * Provides thread state information, including a backtrace and register state.
 */
@implementation PLCrashReportThreadInfo

/**
 * Initialize the crash log thread information.
 */
- (id) initWithThreadNumber: (NSInteger) threadNumber
                stackFrames: (NSArray *) stackFrames
                    crashed: (BOOL) crashed
                  registers: (NSArray *) registers
{
    if ((self = [super init]) == nil)
        return nil;

    _threadNumber = threadNumber;
    _stackFrames = [stackFrames retain];
    _crashed = crashed;
    _registers = [registers retain];

    return self;
}

- (void) dealloc {
    [_stackFrames release];
    [_registers release];
    [super dealloc];
}

@synthesize threadNumber = _threadNumber;
@synthesize stackFrames = _stackFrames;
@synthesize crashed = _crashed;
@synthesize registers = _registers;


@end


/**
 * Crash log stack frame information.
 */
@implementation PLCrashReportStackFrameInfo

/**
 * Initialize with the provided instruction pointer value.
 */
- (id) initWithInstructionPointer: (uint64_t) instructionPointer {
    if ((self = [super init]) == nil)
        return nil;

    _instructionPointer = instructionPointer;

    return self;
}

@synthesize instructionPointer = _instructionPointer;

@end


/**
 * Crash log general purpose register information.
 */
@implementation PLCrashReportRegisterInfo

/**
 * Initialize with the provided name and value.
 */
- (id) initWithRegisterName: (NSString *) registerName registerValue: (uint64_t) registerValue {
    if ((self = [super init]) == nil)
        return nil;

    _registerName = [registerName retain];
    _registerValue = registerValue;

    return self;
}

- (void) dealloc {
    [_registerName release];
    [super dealloc];
}

@synthesize registerName = _registerName;
@synthesize registerValue = _registerValue;

@end
