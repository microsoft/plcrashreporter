/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface PLCrashLogStackFrameInfo : NSObject {
@private
    /** Frame instruction pointer. */
    uint64_t _instructionPointer;
}

- (id) initWithInstructionPointer: (uint64_t) instructionPointer;

/**
 * Frame's instruction pointer.
 */
@property(nonatomic, readonly) uint64_t instructionPointer;

@end


@interface PLCrashLogRegisterInfo : NSObject {
@private
    /** Register name */
    NSString *_registerName;

    /** Register value */
    uint64_t _registerValue;
}

- (id) initWithRegisterName: (NSString *) registerName registerValue: (uint64_t) registerValue;

/**
 * Register name.
 */
@property(nonatomic, readonly) NSString *registerName;

/**
 * Register value.
 */
@property(nonatomic, readonly) uint64_t registerValue;

@end


@interface PLCrashLogThreadInfo : NSObject {
@private
    /** The thread number. Should be unique within a given crash log. */
    NSInteger _threadNumber;

    /** Ordered list of PLCrashLogStackFrame instances */
    NSArray *_stackFrames;

    /** YES if this thread crashed. */
    BOOL _crashed;

    /** List of PLCrashLogRegister instances. Will be empty if _crashed is NO. */
    NSArray *_registers;
}

- (id) initWithThreadNumber: (NSInteger) threadNumber
                stackFrames: (NSArray *) stackFrames
                    crashed: (BOOL) crashed
                  registers: (NSArray *) registers;

/**
 * Application thread number.
 */
@property(nonatomic, readonly) NSInteger threadNumber;

/**
 * Thread backtrace. Provides an array of PLCrashLogStackFrame instances.
 * The array is ordered, last callee to first.
 */
@property(nonatomic, readonly) NSArray *stackFrames;

/**
 * If this thread crashed, set to YES.
 */
@property(nonatomic, readonly) BOOL crashed;

/**
 * State of the general purpose and related registers, as a list of
 * PLCrashLogRegister instances. If this thead did not crash (crashed returns NO),
 * this list will be empty.
 */
@property(nonatomic, readonly) NSArray *registers;

@end
