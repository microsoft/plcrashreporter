/*
 Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 
 This file contains Original Code and/or Modifications of Original Code
 as defined in and that are subject to the Apple Public Source License
 Version 2.0 (the 'License'). You may not use this file except in
 compliance with the License. Please obtain a copy of the License at
 http://www.opensource.apple.com/apsl/ and read it before using this
 file.
 
 The Original Code and all software distributed under the License are
 distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 Please see the License for the specific language governing rights and
 limitations under the License.
*/


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <mach-o/dyld.h>

#include "PLCrashFrameWalker.h"

extern void *unwind_tester_list_x86_64_disable_compact_frame[];

extern int unwind_tester (void *);
static void **unwind_tester_list[] = {
#ifdef __x86_64__
    unwind_tester_list_x86_64_disable_compact_frame,
#elif defined(__i386__)
#endif
    NULL
};

/*
 * Loop over all function pointers in unwind_tester_list
 * and call unwind_tester() on each one.  If it returns
 * false, then that test failed.
 */
bool unwind_test_harness (void) {
    for (void ***suite = unwind_tester_list; *suite != NULL; suite++) {
        for (void **tests = *suite; *tests != NULL; tests++) {
            PLCF_DEBUG("Running %p", *tests);
            if (unwind_tester(*tests))
                return false;
        }
    }
	return true;
}

#define VERIFY_NV_REG(cursor, rnum, value) do { \
    plcrash_greg_t reg; \
    if (plframe_cursor_get_reg(cursor, rnum, &reg) != PLFRAME_ESUCCESS) { \
        PLCF_DEBUG("Failed to fetch non-volatile register!"); \
        __builtin_trap(); \
    } \
    if (reg != value) { \
        PLCF_DEBUG("Incorrect register value"); \
        __builtin_trap(); \
    } \
} while (0)

plcrash_error_t unwind_current_state (plcrash_async_thread_state_t *state, void *context) {
    plframe_cursor_t cursor;
    plcrash_async_image_list_t image_list;

    /* Initialize the image list */
    plcrash_nasync_image_list_init(&image_list, mach_task_self());
    for (uint32_t i = 0; i < _dyld_image_count(); i++)
        plcrash_nasync_image_list_append(&image_list, _dyld_get_image_header(i), _dyld_get_image_name(i));

    /* Initialie our cursor */
    plframe_cursor_init(&cursor, mach_task_self(), state, &image_list);
    
    if (plframe_cursor_next(&cursor) == PLFRAME_ESUCCESS) {
        // now in unwind_to_main
        if (plframe_cursor_next(&cursor) == PLFRAME_ESUCCESS) {
            // now in test function
            if (plframe_cursor_next(&cursor) == PLFRAME_ESUCCESS) {
                // now in unwind_tester
                
                /*
                 * Verify that non-volatile registers have been restored. This replaces the
                 * use of thread state restoration in the original libunwind tests; rather
                 * than letting the unwind_tester() perform these register value tests,
                 * we just do so ourselves
                 */
#ifdef __x86_64__
                VERIFY_NV_REG(&cursor, PLCRASH_X86_64_RBX, 0x1234567887654321);
                VERIFY_NV_REG(&cursor, PLCRASH_X86_64_R12, 0x02468ACEECA86420);
                VERIFY_NV_REG(&cursor, PLCRASH_X86_64_R13, 0x13579BDFFDB97531);
                VERIFY_NV_REG(&cursor, PLCRASH_X86_64_R14, 0x1122334455667788);
                VERIFY_NV_REG(&cursor, PLCRASH_X86_64_R15, 0x0022446688AACCEE);
#else
                // TODO
#endif
                return PLCRASH_ESUCCESS;
            }
        }
    }
    
    return PLCRASH_EINVAL;
}

// called by test function
// we unwind through the test function
// and resume at caller (unwind_tester)
void uwind_to_main () {
    /* Invoke our handler with our current thread state; we use this state to try to roll back the tests
     * and verify that the expected registers are restored. */
    if (plcrash_async_thread_state_current(unwind_current_state, NULL) != PLCRASH_ESUCCESS) {
        __builtin_trap();
    }
}

