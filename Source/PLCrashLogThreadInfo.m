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


/**
 * Crash log stack frame information.
 */
@implementation PLCrashLogStackFrameInfo

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
@implementation PLCrashLogRegisterInfo

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
