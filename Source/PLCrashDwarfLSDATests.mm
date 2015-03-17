/*
 * Author: Joe Ranieri <joe@alacatialabs.com>
 *
 * Copyright (c) 2015 Plausible Labs Cooperative, Inc.
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

#import "PLCrashTestCase.h"
#include "PLCrashDwarfLSDA.hpp"
#include "PLCrashFeatureConfig.h"

#ifdef try
#undef try
#endif
#ifdef catch
#undef catch
#endif

#if PLCRASH_FEATURE_UNWIND_DWARF

using namespace plcrash;
using namespace plcrash::async;

@interface PLCrashAsyncDwarfLSDATests : PLCrashTestCase {
}
@end

@implementation PLCrashAsyncDwarfLSDATests

- (void) setUp {
}

- (void) tearDown {
}

- (void) testSimpleCleanup {
    static const uint8_t lsda[] = {
        0xff,                   // @LPStart Encoding = omit
        0x9b,                   // @TType Encoding = indirect pcrel sdata4
        0x29,                   // @TType base offset
        0x03,                   // Call site Encoding = udata4
        0x0d,                   // Call site table length
        // >> Call Site 1 <<
        0x00, 0x00, 0x00, 0x00, //
        0xa2, 0x00, 0x00, 0x00, // Call between Leh_func_begin0 and Ltmp0
        0x00, 0x00, 0x00, 0x00, // has no landing pad
        0x00,                   // On action: cleanup
    };

    plcrash_async_mobject mobj;
    plcrash_error_t err = plcrash_async_mobject_init(&mobj, mach_task_self(), (pl_vm_address_t)lsda, sizeof(lsda), true);

    dwarf_lsda_info_t info;
    err = dwarf_lsda_info_init<uint64_t>(&info, &mobj, &plcrash_async_byteorder_direct, (pl_vm_address_t)lsda);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to parse LSDA");

    STAssertEquals(info.has_lp_start, false, @"Incorrect has_lp_start");
    STAssertEquals(info.callsites.size(), 1, @"Incorrect number of call sites");

    STAssertEquals(info.callsites[0].start, 0x00, @"callsite 1's start is wrong");
    STAssertEquals(info.callsites[0].length, 0xA2, @"callsite 1's length is wrong");
    STAssertEquals(info.callsites[0].landing_pad_offset, 0, @"callsite 1's landing pad is wrong");
    STAssertEquals(info.callsites[0].actions.size(), 0, @"callsite 1 shouldn't have actions");

    dwarf_lsda_info_free(&info);
    plcrash_async_mobject_free(&mobj);
}

- (void) testExceptionSpecifications {
    // A simple function with an exception specification.
    static const uint8_t lsda[] = {
        0xff,                   // LPStart Encoding = omit
        0x9b,                   // @TType Encoding = indirect pcrel sdata4
        0x99, 0x80, 0x80, 0x00, // @TType base offset
        0x03,                   // Call site Encoding = udata4
        0x0d,                   // Call site table length
        // >> Call Site 1 <<
        0x08, 0x00, 0x00, 0x00, //
        0x0c, 0x00, 0x00, 0x00, // Call between Ltmp0 and Ltmp1
        0x22, 0x00, 0x00, 0x00, // jumps to Ltmp2
        0x01,                   // On action: 1
        // >> Action Record 1 <<
        0x7f,                   // Filter TypeInfo -1
        0x00,                   // No further actions
        // >> Catch TypeInfos <<
        0x0B, 0x00, 0x00, 0x00, // TypeInfo 2
        0x0F, 0x00, 0x00, 0x00, // TypeInfo 1
        // >> Filter TypeInfos <<
        0x01,                   // FilterInfo -1
        0x02,                   // FilterInfo -2
        0x00,
        // >> Non-LSDA data <<
        0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A,
        0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    };

    plcrash_async_mobject mobj;
    plcrash_error_t err = plcrash_async_mobject_init(&mobj, mach_task_self(), (pl_vm_address_t)lsda, sizeof(lsda), true);

    dwarf_lsda_info_t info;
    err = dwarf_lsda_info_init<uint64_t>(&info, &mobj, &plcrash_async_byteorder_direct, (pl_vm_address_t)lsda);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to parse LSDA");

    STAssertEquals(info.has_lp_start, false, @"Incorrect has_lp_start");
    STAssertEquals(info.callsites.size(), 1, @"Incorrect number of call sites");

    STAssertEquals(info.callsites[0].start, 0x08, @"callsite 1's start is wrong");
    STAssertEquals(info.callsites[0].length, 0x0C, @"callsite 1's length is wrong");
    STAssertEquals(info.callsites[0].landing_pad_offset, 0x22, @"callsite 1's landing pad is wrong");
    STAssertEquals(info.callsites[0].actions.size(), 1, @"callsite 1's should have one action");

    STAssertEquals(info.callsites[0].actions[0].kind, DWARF_LSDA_ACTION_KIND_EXCEPTION_SPECIFICATION, @"wrong action kind");
    STAssertEquals(info.callsites[0].actions[0].types.size(), 2, @"wrong type size");
    STAssertEquals(info.callsites[0].actions[0].types[0], 0x3F3F3F3F3F3F3F3F, @"wrong type value");
    STAssertEquals(info.callsites[0].actions[0].types[1], 0x2A2A2A2A2A2A2A2A, @"wrong type value");

    dwarf_lsda_info_free(&info);
    plcrash_async_mobject_free(&mobj);
}

- (void) testMultipleCatches {
    // A function with a try block with several catches.
    static const uint8_t lsda[] = {
        0xff,                   // @LPStart Encoding = omit
        0x9b,                   // @TType Encoding = indirect pcrel sdata4
        0xae, 0x80, 0x00,       // @TType base offset
        0x03,                   // Call site Encoding = udata4
        0x1a,                   // Call site table length
        // >> Call Site 1 <<
        0x00, 0x00, 0x00, 0x00, //
        0x1e, 0x00, 0x00, 0x00, //   Call between Ltmp0 and Ltmp1
        0x00, 0x00, 0x00, 0x00, //     has no landing pad
        0x00,                   //   On action: cleanup
        // >> Call Site 2 <<
        0x1e, 0x00, 0x00, 0x00, //
        0x13, 0x00, 0x00, 0x00, //   Call between Ltmp0 and Ltmp1
        0x36, 0x00, 0x00, 0x00, //     jumps to Ltmp2
        0x05,                   //   On action: 3
        // >> Action Record 1 <<
        0x01,                   //   Catch TypeInfo 1
        0x00,                   //   No further actions
        // >> Action Record 2 <<
        0x02,                   //   Catch TypeInfo 2
        0x7d,                   //   Continue to action 1
        /// >> Action Record 3 <<
        0x03,                   //   Catch TypeInfo 3
        0x7d,                   //   Continue to action 2
        // >> Catch TypeInfos <<
        0x0C, 0x00, 0x00, 0x00, // TypeInfo 3
        0x10, 0x00, 0x00, 0x00, // TypeInfo 2
        0x00, 0x00, 0x00, 0x00, // TypeInfo 1
        // >> Non-LSDA data <<
        0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A,
        0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    };

    plcrash_async_mobject mobj;
    plcrash_error_t err = plcrash_async_mobject_init(&mobj, mach_task_self(), (pl_vm_address_t)lsda, sizeof(lsda), true);

    dwarf_lsda_info_t info;
    err = dwarf_lsda_info_init<uint64_t>(&info, &mobj, &plcrash_async_byteorder_direct, (pl_vm_address_t)lsda);
    STAssertEquals(err, PLCRASH_ESUCCESS, @"Failed to parse LSDA");

    STAssertEquals(info.has_lp_start, false, @"Incorrect has_lp_start");
    STAssertEquals(info.callsites.size(), 2, @"Incorrect number of call sites");

    STAssertEquals(info.callsites[0].start, 0, @"callsite 1's start is wrong");
    STAssertEquals(info.callsites[0].length, 0x1e, @"callsite 1's length is wrong");
    STAssertEquals(info.callsites[0].landing_pad_offset, 0, @"callsite 1's landing pad is wrong");
    STAssertEquals(info.callsites[0].actions.size(), 0, @"callsite 1's shouldn't have actions");

    STAssertEquals(info.callsites[1].start, 0x1e, @"callsite 2's start is wrong");
    STAssertEquals(info.callsites[1].length, 0x13, @"callsite 2's length is wrong");
    STAssertEquals(info.callsites[1].landing_pad_offset, 0x36, @"callsite 2's landing pad is wrong");
    STAssertEquals(info.callsites[1].actions.size(), 3, @"callsite 2's action count is wrong");

    STAssertEquals(info.callsites[1].actions[0].kind, DWARF_LSDA_ACTION_KIND_CATCH, @"wrong action kind");
    STAssertEquals(info.callsites[1].actions[0].types.size(), 1, @"wrong type size");
    STAssertEquals(info.callsites[1].actions[0].types[0], 0x2A2A2A2A2A2A2A2A, @"wrong type value");

    STAssertEquals(info.callsites[1].actions[1].kind, DWARF_LSDA_ACTION_KIND_CATCH, @"wrong action kind");
    STAssertEquals(info.callsites[1].actions[1].types.size(), 1, @"wrong type size");
    STAssertEquals(info.callsites[1].actions[1].types[0], 0x3F3F3F3F3F3F3F3F, @"wrong type value");

    STAssertEquals(info.callsites[1].actions[2].kind, DWARF_LSDA_ACTION_KIND_CATCH, @"wrong action kind");
    STAssertEquals(info.callsites[1].actions[2].types.size(), 1, @"wrong type size");
    STAssertEquals(info.callsites[1].actions[2].types[0], 0, @"wrong type value");

    dwarf_lsda_info_free(&info);
    plcrash_async_mobject_free(&mobj);
}

@end

#endif /* PLCRASH_FEATURE_UNWIND_DWARF */
