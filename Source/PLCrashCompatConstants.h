/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2012-2013 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_COMPAT_CONSTANTS_H
#define PLCRASH_COMPAT_CONSTANTS_H 1

#include <AvailabilityMacros.h>

#include <mach/machine.h>

/*
 * With the introduction of new processor types and subtypes, Apple often does not update the system headers
 * on Mac OS X (and the Simulator). This header provides compatibility defines (and #warnings that will
 * fire when the SDKs are updated to include the required constants.
 */
#define PLCF_COMPAT_HAS_UPDATED_OSX_SDK(sdk_version) (TARGET_OS_MAC && !TARGET_OS_IPHONE) && ((PLCF_MIN_MACOSX_SDK > sdk_version) || (MAC_OS_X_VERSION_MAX_ALLOWED <= sdk_version))


/* ARM64 compact unwind constants. The iPhoneSimulator 7.0 SDK includes the compact unwind enums,
 * but not the actual CPU_TYPE_ARM64 defines, so we must special case it here. */
#if !defined(CPU_TYPE_ARM64) && !TARGET_IPHONE_SIMULATOR
enum {
    UNWIND_ARM64_MODE_MASK                  = 0x0F000000,
    UNWIND_ARM64_MODE_FRAME_OLD             = 0x01000000,
    UNWIND_ARM64_MODE_FRAMELESS             = 0x02000000,
    UNWIND_ARM64_MODE_DWARF                 = 0x03000000,
    UNWIND_ARM64_MODE_FRAME                 = 0x04000000,
    
    UNWIND_ARM64_FRAME_X19_X20_PAIR         = 0x00000001,
    UNWIND_ARM64_FRAME_X21_X22_PAIR         = 0x00000002,
    UNWIND_ARM64_FRAME_X23_X24_PAIR         = 0x00000004,
    UNWIND_ARM64_FRAME_X25_X26_PAIR         = 0x00000008,
    UNWIND_ARM64_FRAME_X27_X28_PAIR         = 0x00000010,
    
    UNWIND_ARM64_FRAMELESS_STACK_SIZE_MASK  = 0x00FFF000,
    UNWIND_ARM64_DWARF_SECTION_OFFSET       = 0x00FFFFFF,
};
#elif PLCF_COMPAT_HAS_UPDATED_OSX_SDK(MAC_OS_X_VERSION_10_8)
# warning UNWIND_ARM64_* constants are now defined by the minimum supported Mac SDK. Please remove this define.
#endif

/* CPU type/subtype constants */
#ifndef CPU_SUBTYPE_ARM_V7S
# define CPU_SUBTYPE_ARM_V7S 11
#elif PLCF_COMPAT_HAS_UPDATED_OSX_SDK(MAC_OS_X_VERSION_10_8)
# warning CPU_SUBTYPE_ARM_V7S is now defined by the minimum supported Mac SDK. Please remove this define.
#endif

#ifndef CPU_TYPE_ARM64
# define CPU_TYPE_ARM64 (CPU_TYPE_ARM | CPU_ARCH_ABI64)
#elif PLCF_COMPAT_HAS_UPDATED_OSX_SDK(MAC_OS_X_VERSION_10_8)
# warning CPU_TYPE_ARM64 is now defined by the minimum supported Mac SDK. Please remove this define.
#endif

#ifndef CPU_SUBTYPE_ARM_V8
# define CPU_SUBTYPE_ARM_V8 13
#elif PLCF_COMPAT_HAS_UPDATED_OSX_SDK(MAC_OS_X_VERSION_10_8)
# warning CPU_SUBTYPE_ARM_V8 is now defined by the minimum supported Mac SDK. Please remove this define.
#endif


#endif /* PLCRASH_COMPAT_CONSTANTS_H */
