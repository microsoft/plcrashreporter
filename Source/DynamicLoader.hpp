/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2013 - 2015 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_DYNAMIC_LOADER_H
#define PLCRASH_DYNAMIC_LOADER_H

#include <mach/task_info.h>

#include "PLCrashMacros.h"
#include "PLCrashAsync.h"

PLCR_CPP_BEGIN_ASYNC_NS

/**
 * An immutable reference to a source of dynamic loader data for
 * a specific target task.
 */
class DynamicLoader {
public:
    /**
     * Attempt to fetch a DynamicLoader reference for @a task, placing the result
     * in @a loader on success.
     *
     * @param task The target task.
     * @param[out] loader On success, will be initialized with a valid DynamicLoader instance.
     *
     * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned.
     *
     * @warning This method is not gauranteed async-safe.
     */
    static plcrash_error_t nasync_get (mach_port_t task, DynamicLoader &loader);
    
    DynamicLoader ();
    ~DynamicLoader ();
    
    /** Copy constructor */
    DynamicLoader (const DynamicLoader &other) : DynamicLoader(other._task, other._dyld_info) {}

    /** Move constructor */
    DynamicLoader (DynamicLoader &&other) : _task(other._task), _dyld_info(other._dyld_info) {
        other._task = MACH_PORT_NULL;
    }
    
    /** Copy assignment operator */
    DynamicLoader &operator= (const DynamicLoader &other) {
        /* Update our task reference */
        set_task(other._task);
        
        /* Update dyld info */
        _dyld_info = other._dyld_info;
        
        return *this;
    }
    
    /** Move assignment operator */
    DynamicLoader &operator= (DynamicLoader &&other) {
        /* Borrow the other instance's task reference */
        _task = other._task;
        other._task = MACH_PORT_NULL;
        
        /* Update dyld info */
        _dyld_info = other._dyld_info;
        
        return *this;
    }
    
    /**
     * Return the task dyld info.
     */
    struct task_dyld_info dyld_info () { return _dyld_info; }
    
private:
    DynamicLoader (mach_port_t task, struct task_dyld_info dyld_info);

    void set_task (mach_port_t new_task);
    
    /** The task containing the fetched dyld info. */
    mach_port_t _task;

    /** The dyld info fetched from _task */
    struct task_dyld_info _dyld_info;
};

PLCR_CPP_END_ASYNC_NS

#endif /* PLCRASH_DYNAMIC_LOADER_H */
