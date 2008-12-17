/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashLogThreadInfo.h"

/**
 * Crash log per-thread state information.
 *
 * Provides thread state information, including a backtrace
 * and register state.
 */
@implementation PLCrashLogThreadInfo

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
