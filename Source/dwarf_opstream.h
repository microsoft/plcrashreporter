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

#ifndef PLCRASH_ASYNC_DWARF_OPSTREAM_H
#define PLCRASH_ASYNC_DWARF_OPSTREAM_H

#include <cstddef>

extern "C" {
#include "PLCrashAsync.h"
#include "PLCrashAsyncMObject.h"
#include "PLCrashAsyncDwarfPrimitives.h"
}

/**
 * @internal
 * @ingroup plcrash_async_dwarf_opstream
 * @{
 */

namespace plcrash {

/**
 * A simple opcode stream reader for use with DWARF opcode/CFA evaluation.
 */
class dwarf_opstream {
    /** Current position within the op stream */
    void *_p;
    
    /** Backing memory object */
    plcrash_async_mobject_t *_mobj;
    
    /** Target-relative starting address within the memory object */
    pl_vm_address_t _start;
    
    /** Target-relative end address within the memory object */
    pl_vm_address_t _end;

    /** Locally mapped starting address. */
    void *_instr;

    /** Locally mapped ending address. */
    void *_instr_max;
    
    /** Byte order for the byte stream */
    const plcrash_async_byteorder_t *_byteorder;
    
public:
    plcrash_error_t init (plcrash_async_mobject_t *mobj,
                          const plcrash_async_byteorder_t *byteorder,
                          pl_vm_address_t address,
                          pl_vm_off_t offset,
                          pl_vm_size_t length);
    
    template <typename V> inline bool read_intU (V *result);
    inline bool read_uleb128 (uint64_t *result);
    inline bool read_sleb128 (int64_t *result);
    inline bool skip (int16_t offset);
};


/**
 * Read a value of type and size @a V from the stream, verifying that the read will not overrun
 * the mapped range and advancing the stream position past the read value.
 *
 * @warning Multi-byte values (either 2, 4, or 8 bytes in size) will be byte swapped.
 *
 * @param result The destination to which the result will be written.
 *
 * @return Returns true on success, or false if the read would exceed the boundry specified by @a maxpos.
 */
template <typename V> inline bool dwarf_opstream::read_intU (V *result) {
    if (_p < _instr)
        return false;

    if ((uint8_t *)_instr_max - (uint8_t *)_p < sizeof(V)) {
        return false;
    }
    
    *result = *((V *)_p);
    
    switch (sizeof(V)) {
        case 2:
            *result = _byteorder->swap16(*result);
            break;
        case 4:
            *result = _byteorder->swap32(*result);
            break;
        case 8:
            *result = _byteorder->swap64(*result);
            break;
        default:
            break;
    }
    
    _p = ((uint8_t *)_p) + sizeof(V);
    return true;
}
    
/**
 * Read a ULEB128 value from the stream, verifying that the read will not overrun
 * the mapped range and advancing the stream position past the read value.
 *
 * @param result The destination to which the result will be written.
 *
 * @return Returns true on success, or false if the read would exceed the boundry specified by @a maxpos.
 */
inline bool dwarf_opstream::read_uleb128 (uint64_t *result) {
    plcrash_error_t err;
    pl_vm_off_t offset = ((uint8_t *)_p - (uint8_t *)_instr);
    pl_vm_size_t lebsize;

    if ((err = plcrash_async_dwarf_read_uleb128(_mobj, _start, offset, result, &lebsize)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Read of ULEB128 value failed with %u", err);
        return false;
    }

    _p = (uint8_t *)_p + lebsize;
    return true;
}

/**
 * Read a SLEB128 value from the stream, verifying that the read will not overrun
 * the mapped range and advancing the stream position past the read value.
 *
 * @param result The destination to which the result will be written.
 *
 * @return Returns true on success, or false if the read would exceed the boundry specified by @a maxpos.
 */
inline bool dwarf_opstream::read_sleb128 (int64_t *result) {
    plcrash_error_t err;
    pl_vm_off_t offset = ((uint8_t *)_p - (uint8_t *)_instr);
    pl_vm_size_t lebsize;
    
    if ((err = plcrash_async_dwarf_read_sleb128(_mobj, _start, offset, result, &lebsize)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Read of ULEB128 value failed with %u", err);
        return false;
    }
    
    _p = (uint8_t *)_p + lebsize;
    return true;
}

/**
 * Apply the given offset to the instruction position, returning false
 * if the position falls outside of the bounds of the mapped region.
 */
inline bool dwarf_opstream::skip (int16_t offset) {
    void *p = ((uint8_t *)_p) + offset;
    if (p < _instr || p > _instr_max)
        return false;

    _p = p;
    return true;
}

    
}
    
/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_OPSTREAM_H */
