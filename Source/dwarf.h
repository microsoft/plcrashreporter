//
//  dwarf_encoding.h
//  CrashReporter
//
//  Created by Landon Fuller on 6/1/13.
//
//

#ifndef CrashReporter_dwarf_encoding_h
#define CrashReporter_dwarf_encoding_h



#endif
/*
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

#ifndef PLCRASH_ASYNC_DWARF_H
#define PLCRASH_ASYNC_DWARF_H 1

#include "PLCrashAsync.h"
#include "PLCrashAsyncImageList.h"
#include "PLCrashAsyncThread.h"

#include <mach-o/compact_unwind_encoding.h>

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @defgroup plcrash_async_dwarf_constants DWARF Constants
 *
 * Private DWARF Constants.
 *
 * @{
 */

/**
 * Exception handling pointer encoding constants, as defined by the LSB Specification:
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html
 *
 * The upper 4 bits indicate how the value is to be applied. The lower 4 bits indicate the format of the data.
 */
typedef enum {
    /**
     * Value is an indirect reference. This value is not specified by the LSB, and appears to be a
     * GCC extension; unfortunately, the intended use is not clear:
     *
     * - Apple's implementation of libunwind treats this as an indirected reference to a target-width pointer value,
     *   as does the upstream libunwind.
     * - LLDB does not appear to support indirect encoding at all.
     * - LLVM's asm printer decodes it as an independent flag on the encoding type value; eg, DW_EH_PE_indirect | DW_EH_PE_uleb128 | DW_EH_PE_pcrel
     *   LLVM/clang does not seem to otherwise emit this value.
     * - GDB explicitly does not support indirect encodings.
     *
     * For our purposes, we treat the value as per LLVM's asm printer, and may re-evaluate if the indirect encoding
     * is ever seen in the wild.
     */
    DW_EH_PE_indirect = 0x80,
    
    /** No value is present. */
    DW_EH_PE_omit = 0xff,
    
    /** The value is a literal pointer whose size is determined by the architecture. */
    DW_EH_PE_absptr = 0x00,
    
    /** Unsigned value encoded using LEB128 as defined by DWARF Debugging Information Format, Revision 2.0.0. */
    DW_EH_PE_uleb128 = 0x01,
    
    /** Unsigned 16-bit value */
    DW_EH_PE_udata2 = 0x02,
    
    /* Unsigned 32-bit value */
    DW_EH_PE_udata4 = 0x03, /* unsigned 32-bit value */
    
    /** Unsigned 64-bit value */
    DW_EH_PE_udata8 = 0x04, /* unsigned 64-bit value */
    
    /** Signed value encoded using LEB128 as defined by DWARF Debugging Information Format, Revision 2.0.0. */
    DW_EH_PE_sleb128 = 0x09, /* signed LE base-128 value */
    
    /** Signed 16-bit value */
    DW_EH_PE_sdata2 = 0x0a, /* signed 16-bit value */
    
    /** Signed 32-bit value */
    DW_EH_PE_sdata4 = 0x0b, /* signed 32-bit value */

    /** Signed 64-bit value */
    DW_EH_PE_sdata8 = 0x0c, /* signed 64-bit value */
    
    /** Absolute value */
    DW_EH_PE_absptr = 0x00, /* absolute value */
    
    /** Value is relative to the current program counter. */
    DW_EH_PE_pcrel = 0x10,
    
    /** Value is relative to the beginning of the .text section. This is not used on Darwin. */
    DW_EH_PE_textrel = 0x20,

    /** Value is relative to the beginning of the .got or .eh_frame_hdr section. This is not used on Darwin. */
    DW_EH_PE_datarel = 0x30,

    /** Value is relative to the beginning of the function. This is not used on Darwin. */
    DW_EH_PE_funcrel = 0x40,
    
    /** Value is aligned to an address unit sized boundary. This is not used on Darwin. */
    DW_EH_PE_aligned = 0x50,
} DW_EH_PE_t;

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_H */