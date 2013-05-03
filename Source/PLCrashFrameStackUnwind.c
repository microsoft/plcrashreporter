/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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

#include "PLCrashFrameStackUnwind.h"
#include "PLCrashAsync.h"

/**
 * Fetch the next frame, assuming a valid frame pointer in @a cursor's current frame.
 *
 * @param cursor A cursor instance initialized with plframe_cursor_init();
 * @param frame The new frame to be initialized.
 *
 * @return Returns PLFRAME_ESUCCESS on success, PLFRAME_ENOFRAME is no additional frames are available, or a standard plframe_error_t code if an error occurs.
 */
plframe_error_t plframe_cursor_read_frame_ptr (task_t task, const plframe_stackframe_t *current_frame, plframe_stackframe_t *next_frame) {
    /* Determine the appropriate type width for the target thread */
    union {
        uint64_t greg64[2];
        uint32_t greg32[2];
    } regs;
    void *dest;
    size_t len;

    bool x64 = plcrash_async_thread_state_get_greg_size(&current_frame->thread_state) == sizeof(uint64_t);
    if (x64) {
        dest = regs.greg64;
        len = sizeof(regs.greg64);
    } else {
        dest = regs.greg32;
        len = sizeof(regs.greg32);
    }
    
    /* Fetch and verify the current frame pointer. A frame pointer of 0 is a known missing frame. */
    plcrash_greg_t fp;

    if (!plframe_regset_isset(current_frame->valid_registers, PLCRASH_REG_FP))
        return PLFRAME_ENOFRAME;

    fp = plcrash_async_thread_state_get_reg(&current_frame->thread_state, PLCRASH_REG_FP);
    if (fp == 0x0)
        return PLFRAME_ENOFRAME;


    /* Read the registers off the stack */
    kern_return_t kr;
    kr = plcrash_async_read_addr(task, (pl_vm_address_t) fp, dest, len);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Failed to read frame: %d", kr);
        return PLFRAME_EBADFRAME;
    }
    
    /* Initialize the new frame, deriving state from the previous frame. */
    *next_frame = *current_frame;
    plframe_regset_zero(&next_frame->valid_registers);
    plframe_regset_set(&next_frame->valid_registers, PLCRASH_REG_FP);
    plframe_regset_set(&next_frame->valid_registers, PLCRASH_REG_IP);

    if (x64) {
        plcrash_async_thread_state_set_reg(&next_frame->thread_state, PLCRASH_REG_FP, regs.greg64[0]);
        plcrash_async_thread_state_set_reg(&next_frame->thread_state, PLCRASH_REG_IP, regs.greg64[1]);
    } else {
        plcrash_async_thread_state_set_reg(&next_frame->thread_state, PLCRASH_REG_FP, regs.greg32[0]);
        plcrash_async_thread_state_set_reg(&next_frame->thread_state, PLCRASH_REG_IP, regs.greg32[1]);
    }

    return PLFRAME_ESUCCESS;
}