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
#include <inttypes.h>

#include <mach-o/dyld.h>

#include "PLCrashFrameWalker.h"

extern void *unwind_tester_list_x86_64_disable_compact_frame[];

extern void *unwind_tester_list_x86_64_frame[];
extern void *unwind_tester_list_x86_64_frame_compact[];
extern void *unwind_tester_list_x86_64_frame_eh_frame[];
extern void *unwind_tester_list_x86_64_frame_compact_eh_frame[];

extern int unwind_tester (void *);
extern void unwind_tester_target_ip (void);

struct unwind_test_case {
    /* A list of targetable test cases */
    void *test_list;

    /* If true, the test cases vends eh_frame/compact unwind data,
     * and we should validate that callee-preserved registers were
     * correctly restored */
    bool restores_callee_registers;
};

static struct unwind_test_case unwind_test_cases[] = {
#ifdef __x86_64__
    { unwind_tester_list_x86_64_disable_compact_frame, true },  /* DWARF unwinding (no compact frame data) */
    { unwind_tester_list_x86_64_frame, false },                 /* frame-based unwinding, no eh_frame/compact unwind data */
    { unwind_tester_list_x86_64_frame_compact, true },          /* frame-based compact unwinding */
    { unwind_tester_list_x86_64_frame_eh_frame, true },         /* frame-based eh_frame unwinding */
    { unwind_tester_list_x86_64_frame_compact_eh_frame, true }, /* frame-based eh_fame+compact unwinding */
#elif defined(__i386__)
#endif
    { NULL, false }
};


/*
 * We abuse global state to pass configuration down to the test result handling
 * without having to modify all the existing test cases. This means the tests
 * are not re-entrant, but somehow I suspect that's OK.
 */
struct  {
    /** The current test case */
    struct unwind_test_case *test_case;
} global_harness_state;

/*
 * Loop over all function pointers in unwind_tester_list
 * and call unwind_tester() on each one.  If it returns
 * false, then that test failed.
 */
bool unwind_test_harness (void) {
    for (struct unwind_test_case *tc = unwind_test_cases; tc->test_list != NULL; tc++) {
        global_harness_state.test_case = tc;
        for (void **tests = tc->test_list; *tests != NULL; tests++) {
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
                
                /* Verify that we unwound to the correct IP */
                plcrash_greg_t ip;
                if (plframe_cursor_get_reg(&cursor, PLCRASH_REG_IP, &ip) != PLFRAME_ESUCCESS) {
                    PLCF_DEBUG("Could not fetch IP from register state");
                    return PLCRASH_EINTERNAL;
                }
                if (ip != (plcrash_greg_t) unwind_tester_target_ip) {
                    PLCF_DEBUG("Incorrect IP. ip=%" PRIx64 " target_ip=%" PRIx64, (uint64_t) ip, (uint64_t) unwind_tester_target_ip);
                    return PLCRASH_EINTERNAL;
                }

                /*
                 * For tests using DWARF or compact unwinding, verify that non-volatile registers have been restored.
                 * This replaces the use of thread state restoration in the original libunwind tests; rather
                 * than letting the unwind_tester() perform these register value tests,
                 * we just do so ourselves
                 */
                if (!global_harness_state.test_case->restores_callee_registers)
                    return PLCRASH_ESUCCESS;

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

