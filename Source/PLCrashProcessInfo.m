/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2013 Plausible Labs Cooperative, Inc.
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

#import "PLCrashProcessInfo.h"
#import "PLCrashAsync.h"

#import <unistd.h>

#include <sys/types.h>
#include <sys/sysctl.h>

/**
 * @internal
 * @ingroup plcrash_host
 *
 * @{
 */

/**
 * The PLCrashProcessInfo provides methods to access basic information about a target process.
 */
@implementation PLCrashProcessInfo

@synthesize processID = _processID;
@synthesize processName = _processName;
@synthesize parentProcessID = _parentProcessID;

@synthesize traced = _traced;
@synthesize startTime = _startTime;

/**
 * Return the current process info of the calling process. Note that these values
 * will be fetched once, and the returned instance is immutable.
 */
+ (instancetype) currentProcessInfo {
    return [[[self alloc] initWithProcessID: getpid()] autorelease];
}

/**
 * Initialize a new instance with the process info for the process with @a pid. Returns nil if
 * @a pid does not reference a valid process.
 *
 * @param pid The process identifier of the target process.
 */
- (instancetype) initWithProcessID: (pid_t) pid {
    if ((self = [super init]) == nil)
        return nil;

    /* Fetch the kinfo_proc structure for the target pid */
    int process_info_mib[] = {
        CTL_KERN,
        KERN_PROC,
        KERN_PROC_PID,
        pid
    };
    struct kinfo_proc process_info;
    size_t process_info_len = sizeof(process_info);

    /* This should always succeed unless the process is not found. */
    if (sysctl(process_info_mib, sizeof(process_info_mib)/sizeof(process_info_mib[0]), &process_info, &process_info_len, NULL, 0) != 0) {
        if (errno == ENOENT)
            PLCF_DEBUG("Unexpected sysctl error %d: %s", errno, strerror(errno));
        
        [self release];
        return nil;
    }
    
    /* Extract the pertinent values */
    if (process_info.kp_proc.p_flag & P_TRACED)
        _traced = true;
    
    _processID = pid;
    _processName = [[NSString alloc] initWithBytes: process_info.kp_proc.p_comm length: strlen(process_info.kp_proc.p_comm) encoding: NSUTF8StringEncoding];
    _parentProcessID = process_info.kp_eproc.e_ppid;
    _startTime = process_info.kp_proc.p_starttime;

    return self;
}

- (void) dealloc {
    [_processName release];
    [super dealloc];
}

@end

/**
 * @}
 */
