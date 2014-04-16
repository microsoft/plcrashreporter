/*
 Copyright (c) 2013 Plausible Labs Cooperative, Inc. All rights reserved.
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

#include "PLCrashFrameStackUnwind.h"
#include "PLCrashFrameCompactUnwind.h"
#include "PLCrashFrameDWARFUnwind.h"

#include "PLCrashFeatureConfig.h"

/* Enable libunwind verification on supported platforms.
 * unw_resume() et al are unsupported on 32-bit ARM */
#if !defined(__arm__)
#include "libunwind.h"
#define LIBUNWIND_VERIFICATION 1
#endif

/* Compiler warnings will trigger on unused static variables and functions
 * on architectures where the tests are not currently supported.
 *
 * This define will explicitly mark those members as unused, quieting the compiler
 * warnings */
#if defined(__arm__)
#define UNWIND_ARCH_UNSUPPORTED_UNUSED_ATTR __attribute__((unused))
#else
#define UNWIND_ARCH_UNSUPPORTED_UNUSED_ATTR
#endif

extern void *unwind_tester_list_x86_64_frame[];
extern void *unwind_tester_list_x86_64_frameless[];
extern void *unwind_tester_list_x86_64_frameless_big[];
extern void *unwind_tester_list_x86_64_unusual[];

extern void *unwind_tester_list_x86_frame[];
extern void *unwind_tester_list_x86_frameless[];
extern void *unwind_tester_list_x86_frameless_big[];
extern void *unwind_tester_list_x86_unusual[];

extern void *unwind_tester_list_arm64_frame[];
extern void *unwind_tester_list_arm64_frameless[];

extern int unwind_tester (void *test, void **sp);
extern void *unwind_tester_target_ip;

void uwind_tester_invoke ();

static plframe_cursor_frame_reader_t *frame_readers_frame[] UNWIND_ARCH_UNSUPPORTED_UNUSED_ATTR = {
    plframe_cursor_read_frame_ptr,
    NULL
};

static plframe_cursor_frame_reader_t *frame_readers_compact[] UNWIND_ARCH_UNSUPPORTED_UNUSED_ATTR = {
#if PLCRASH_FEATURE_UNWIND_COMPACT
    plframe_cursor_read_compact_unwind,
#endif
    NULL
};

static plframe_cursor_frame_reader_t *frame_readers_dwarf[] UNWIND_ARCH_UNSUPPORTED_UNUSED_ATTR = {
#if PLCRASH_FEATURE_UNWIND_DWARF
    plframe_cursor_read_dwarf_unwind,
#endif
    NULL
};

struct unwind_test_case {
    /* A list of targetable test cases */
    void *test_list;

    /* If true, the test cases vends eh_frame/compact unwind data,
     * and we should validate that callee-preserved registers were
     * correctly restored */
    bool restores_callee_registers;

    /** Frame reader(s) to use for this test, or NULL to use the default set */
    plframe_cursor_frame_reader_t **frame_readers_dwarf;
    
    /* The number of frames that must be unwound prior to reaching the test function.
     * This may be larger in the case that the test case requires a trampoline
     * prior to calling the unwind function. */
    uint32_t intermediate_frames;
    
    /* If true, the test function is incompatible with Apple's unwinder. Currently, the only
     * case where this is true is with our ARM64 leaf unwind functions.
     *
     * To test leaf unwinding, we implement a custom trampoline to allow preserving/restoring
     * the leaf function's state despite the leaf function not actually being a leaf function --
     * we issue a function call back into our test harness (via the trampoline).
     *
     * To implement this, we make use of DWARF's CIE return_address_register, which allows us to
     * preserve the ARM64 link register while also correctly unwinding frome the trampoline. However,
     * Apple's libunwind ignores return_address_register, preventing its use for our leaf
     * tests.
     *
     * A request for supporting return_address_register was filed as rdar://15223612.
     */
    bool skip_libunwind_verification;

    /** The stack pointer value that should be restored. This is populated by
     * the unwind_tester() */
    void *expected_sp;
};

static struct unwind_test_case unwind_test_cases[] = {
#ifdef __x86_64__
    /* frame-based unwinding */
    { unwind_tester_list_x86_64_frame,      false,  frame_readers_frame,    2 },
    { unwind_tester_list_x86_64_frame,      true,   frame_readers_compact,  2 },
    { unwind_tester_list_x86_64_frame,      true,   frame_readers_dwarf,    2 },
    { unwind_tester_list_x86_64_frame,      true,   NULL,                   2 },
    
    /* frameless unwinding */
    { unwind_tester_list_x86_64_frameless,  true,   frame_readers_compact,  2 },
    { unwind_tester_list_x86_64_frameless,  true,   frame_readers_dwarf,    2 },
    { unwind_tester_list_x86_64_frameless,  true,   NULL,                   2 },
    
    /* frameless unwinding (large frames) */
    { unwind_tester_list_x86_64_frameless_big,  true,   frame_readers_compact,  2 },
    { unwind_tester_list_x86_64_frameless_big,  true,   frame_readers_dwarf,    2 },
    { unwind_tester_list_x86_64_frameless,      true,   NULL,                   2 },

    /* Unusual test cases. These can't be run with /only/ the compact unwinder, as
     * some of the tests rely on constructs that cannot be represented with DWARF. */
    { unwind_tester_list_x86_64_unusual,      true,   frame_readers_dwarf,  2 },
    { unwind_tester_list_x86_64_unusual,      true,   NULL,                 2 },

#elif defined(__i386__)
    /* frame-based unwinding */
    { unwind_tester_list_x86_frame,      false,  frame_readers_frame,   2 },
    { unwind_tester_list_x86_frame,      true,   frame_readers_compact, 2 },
    { unwind_tester_list_x86_frame,      true,   frame_readers_dwarf,   2 },
    { unwind_tester_list_x86_frame,      true,   NULL,                  2 },
    
    /* frameless unwinding */
    { unwind_tester_list_x86_frameless,  true,   frame_readers_compact, 2 },
    { unwind_tester_list_x86_frameless,  true,   frame_readers_dwarf,   2 },
    { unwind_tester_list_x86_frameless,  true,   NULL,                  2 },
    
    /* frameless unwinding (large frames) */
    { unwind_tester_list_x86_frameless_big,  true,   frame_readers_compact, 2 },
    { unwind_tester_list_x86_frameless_big,  true,   frame_readers_dwarf,   2 },
    { unwind_tester_list_x86_frameless,      true,   NULL,                  2 },

    /* Unusual test cases. These can't be run with /only/ the compact unwinder, as
     * some of the tests rely on constructs that cannot be represented with DWARF. */
    { unwind_tester_list_x86_unusual,      true,   frame_readers_dwarf, 2 },
    { unwind_tester_list_x86_unusual,      true,   NULL,                2 },
#elif defined(__arm64__)
    /* frame-based unwinding */
    { unwind_tester_list_arm64_frame,   false,  frame_readers_frame,        2 },
    { unwind_tester_list_arm64_frame,   true,   frame_readers_compact,      2 },
    { unwind_tester_list_arm64_frame,   true,   frame_readers_dwarf,        2 },
    { unwind_tester_list_arm64_frame,   true,   NULL,                       2 },

    /* frameless unwinding */
    { unwind_tester_list_arm64_frameless,  true,   frame_readers_compact,   3,  true },
    //{ unwind_tester_list_arm64_frameless,  true,   frame_readers_dwarf,     3,  true },
    //{ unwind_tester_list_arm64_frameless,  true,   NULL,                    3,  true },
#endif
    { NULL, false }
};


/*
 * We abuse global state to pass configuration down to the test result handling
 * without having to modify all of Apple's test cases. This means the tests
 * are not re-entrant, but somehow I suspect that's OK.
 */
static struct  {
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
            int ret;
            if ((ret = unwind_tester(*tests, &tc->expected_sp)) != 0) {
                PLCF_DEBUG("Tester returned error %d for %p", ret, *tests);
                __builtin_trap();
            }
        }
    }
    
	return true;
}

/**
 * Assert the given @a regnum's value in @a cursor, triggering a trap if @a regnum is unavailable
 * from @a cursor, or does not match @a expectedValue.
 *
 * @param cursor The cursor from which the register value will be fetched.
 * @param regnum The register number to be verified.
 * @param expectedValue The expected register value.
 */
static void assert_register_value (plframe_cursor_t *cursor, plcrash_regnum_t regnum, uint64_t expectedValue) {
    plcrash_greg_t reg;
    if (plframe_cursor_get_reg(cursor, regnum, &reg) != PLFRAME_ESUCCESS) {
        PLCF_DEBUG("Failed to fetch non-volatile register!");
        __builtin_trap();
    }

    if (reg != expectedValue) {
        PLCF_DEBUG("Incorrect register value (%" PRIx64 " != %" PRIx64 ")", (uint64_t)reg, (uint64_t)expectedValue);
        __builtin_trap();
    }
}

/**
 * Perform unwinding tests, using @a state as the initial thread state.
 *
 * @param state The initial thread state to be used for the unwinding tests.
 * @param context Unused.
 */
static plcrash_error_t perform_unwind_test (plcrash_async_thread_state_t *state) {
    plframe_cursor_t cursor;
    plcrash_async_image_list_t image_list;
    plframe_cursor_frame_reader_t **readers = global_harness_state.test_case->frame_readers_dwarf;
    size_t reader_count = 0;
    plframe_error_t err;

    /* Determine the number of frame readers */
    if (readers != NULL) {
        for (reader_count = 0; readers[reader_count] != NULL; reader_count++) {
        }
    }

    /* Initialize the image list */
    plcrash_nasync_image_list_init(&image_list, mach_task_self());
    for (uint32_t i = 0; i < _dyld_image_count(); i++)
        plcrash_nasync_image_list_append(&image_list, _dyld_get_image_header(i), _dyld_get_image_name(i));

    /* Initialie our cursor */
    plframe_cursor_init(&cursor, mach_task_self(), state, &image_list);

    /* Walk the frames until we hit the test function */
    for (uint32_t i = 0; i < global_harness_state.test_case->intermediate_frames; i++) {
        if ((err = plframe_cursor_next(&cursor)) != PLFRAME_ESUCCESS) {
            PLCF_DEBUG("Step failed: %d", err);
            return PLCRASH_EINVAL;
        }
    }

    /* Now in test function; Unwind using the specified readers */
    if (readers != NULL) {
        /* Issue the read */
        err = plframe_cursor_next_with_readers(&cursor, global_harness_state.test_case->frame_readers_dwarf, reader_count);
    } else {
        /* Use default readers */
        err = plframe_cursor_next(&cursor);
    }
    
    if (err != PLFRAME_ESUCCESS) {
        PLCF_DEBUG("Step within test function failed: %d", err);
        return PLFRAME_EINVAL;
    }

    /* Now in unwind_tester; verify that we unwound to the correct IP */
    plcrash_greg_t ip;
    if (plframe_cursor_get_reg(&cursor, PLCRASH_REG_IP, &ip) != PLFRAME_ESUCCESS) {
        PLCF_DEBUG("Could not fetch IP from register state");
        __builtin_trap();
    }
    if (ip != (plcrash_greg_t) unwind_tester_target_ip) {
        PLCF_DEBUG("Incorrect IP. ip=%" PRIx64 " target_ip=%" PRIx64, (uint64_t) ip, (uint64_t) unwind_tester_target_ip);
        __builtin_trap();
    }

    /*
     * For tests using DWARF or compact unwinding, verify that non-volatile registers have been restored.
     * This replaces the use of thread state restoration in the original libunwind tests; rather
     * than letting the unwind_tester() perform these register value tests,
     * we just do so ourselves
     */
    if (!global_harness_state.test_case->restores_callee_registers)
        return PLCRASH_ESUCCESS;

    assert_register_value(&cursor, PLCRASH_REG_SP, (plcrash_greg_t)global_harness_state.test_case->expected_sp);
#ifdef __x86_64__
    assert_register_value(&cursor, PLCRASH_X86_64_RBX, 0x1234567887654321);
    assert_register_value(&cursor, PLCRASH_X86_64_R12, 0x02468ACEECA86420);
    assert_register_value(&cursor, PLCRASH_X86_64_R13, 0x13579BDFFDB97531);
    assert_register_value(&cursor, PLCRASH_X86_64_R14, 0x1122334455667788);
    assert_register_value(&cursor, PLCRASH_X86_64_R15, 0x0022446688AACCEE);
#elif (__i386__)
    assert_register_value(&cursor, PLCRASH_X86_EBX, 0x12344321);
    assert_register_value(&cursor, PLCRASH_X86_ESI, 0x56788765);
    assert_register_value(&cursor, PLCRASH_X86_EDI, 0xABCDDCBA);
#elif (__arm64__)
    assert_register_value(&cursor, PLCRASH_ARM64_X19, 0x1234567887654321);
    assert_register_value(&cursor, PLCRASH_ARM64_X20, 0x02468ACEECA86420);
    assert_register_value(&cursor, PLCRASH_ARM64_X21, 0x13579BDFFDB97531);
    assert_register_value(&cursor, PLCRASH_ARM64_X22, 0x1122334455667788);
    assert_register_value(&cursor, PLCRASH_ARM64_X23, 0x0022446688AACCEE);
    assert_register_value(&cursor, PLCRASH_ARM64_X24, 0x0033557799BBDDFF);
    assert_register_value(&cursor, PLCRASH_ARM64_X25, 0x00446688AACCEE00);
    assert_register_value(&cursor, PLCRASH_ARM64_X26, 0x006688AACCEEFF11);
    assert_register_value(&cursor, PLCRASH_ARM64_X27, 0x0088AACCEEFF1133);
    assert_register_value(&cursor, PLCRASH_ARM64_X28, 0xCAFEDEADF00DBEEF);
#endif
    return PLCRASH_ESUCCESS;
}

/**
 * Implementation of plcrash_async_thread_state_current_callback used by unwind_tester_invoke(); calls through
 * to perform_unwind_test().
 *
 * @param state The thread state acquired by plcrash_async_thread_state_current().
 * @param context Unused.
 */
static plcrash_error_t unwind_tester_invoke_getcontext (plcrash_async_thread_state_t *state, void *context) {
    return perform_unwind_test(state);
}

/**
 * Test function entry point.
 *
 * This is called by our test functions to trigger unwinding. We use plcrash_async_thread_state_current() to collect
 * the current thread's state, handing it to perform_unwind_test(), which will actually use our plframe_cursor_t API
 * to step through the test function and validate the result.
 *
 * After successful validation, and depending on the target platform and test configuration, this function will do
 * one of two things:
 *  - If supported by the platform and the test, Apple's libunwind will be used to unwind the current thread back to
 *    unwind_tester(), prior to the calling of the test. This is intended as a sanity check of our hand-coded
 *    unwind data.
 *  - If libunwind is not supported (on the platform, or for the test in quesiton), then we simply return and let
 *    the test function itself return to its caller.
 */
void uwind_tester_invoke () {
    /* Invoke our handler with our current thread state; we use this state to try to roll back the tests
     * and verify that the expected registers are restored. */
    if (plcrash_async_thread_state_current(unwind_tester_invoke_getcontext, NULL) != PLCRASH_ESUCCESS) {
        __builtin_trap();
    }

    /* Now use libunwind to verify that our test data can be unwound sucessfully. This will unwind the current
     * thread to the unwind_tester, and we'll never return from this function */
#ifdef LIBUNWIND_VERIFICATION
    if (global_harness_state.test_case->skip_libunwind_verification)
        return;

    unw_cursor_t cursor;
	unw_context_t uc;
	
	unw_getcontext(&uc);
	unw_init_local(&cursor, &uc);
    
    /* Walk the frames until we hit the test function. Unlike our unwinder, the first frame is implicitly
     * available -- a step isn't required, and so we skip one call to unw_step(). */
    for (uint32_t i = 1; i < global_harness_state.test_case->intermediate_frames; i++) {
        int ret;
        if ((ret = unw_step(&cursor)) <= 0) {
            PLCF_DEBUG("Step %" PRIu32 " failed: %d", i, ret);
            __builtin_trap();
        }
    }


    /* Once inside the test implementation, resume */
    if (unw_step(&cursor) > 0) {
        unw_resume(&cursor);
    }

	/* This should be unreachable */
	__builtin_trap();
#endif
}

