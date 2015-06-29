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

#include "DynamicLoader.hpp"

using namespace plcrash::async;

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
plcrash_error_t DynamicLoader::nasync_get (mach_port_t task, DynamicLoader &loader) {
    kern_return_t kr;

    /* Try to fetch the task info. */
    task_dyld_info_data_t data;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    
    kr = task_info(task, TASK_DYLD_INFO, (task_info_t) &data, &count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of TASK_DYLD_INFO failed with Mach error: %d", kr);
        return PLCRASH_EINTERNAL;
    }
    
    /* Success! Initialize via our copy/move constructor */
    loader = DynamicLoader(task, data);
    return PLCRASH_ESUCCESS;
}

/**
 * Construct a new DynamicLoader instance with the provided target @a task and the task's @a dyld_info.
 *
 * @param task The target task.
 * @param dyld_info The TASK_DYLD_INFO data fetched from @a task.
 */
DynamicLoader::DynamicLoader (mach_port_t task, struct task_dyld_info dyld_info) : _task(MACH_PORT_NULL), _dyld_info(dyld_info) {
    /* Add a proper refcount for the task */
    set_task(task);
}

/**
 * Construct a new, empty dynamic loader instance.
 */
DynamicLoader::DynamicLoader () : _task(MACH_PORT_NULL) {
    plcrash_async_memset(&_dyld_info, 0, sizeof(_dyld_info));
}


/**
 * Update the backing task port, handling reference counting of our send right.
 *
 * @param new_task The new task port value. May be MACH_PORT_NULL or MACH_PORT_DEAD.
 */
void DynamicLoader::set_task (mach_port_t new_task) {
    /* Is there anything to do? */
    if (_task == new_task)
        return;
    
    /* Drop our send right reference. */
    if (MACH_PORT_VALID(_task))
    mach_port_mod_refs(mach_task_self(), _task, MACH_PORT_RIGHT_SEND, -1);
    
    /* Save the new task port value */
    _task = new_task;
    
    /* Acquire a send right reference, keeping the new task port alive. */
    if (MACH_PORT_VALID(_task))
    mach_port_mod_refs(mach_task_self(), _task, MACH_PORT_RIGHT_SEND, 1);
}

DynamicLoader::~DynamicLoader () {
    /* Discard our task port reference, if any */
    set_task(MACH_PORT_NULL);
}
