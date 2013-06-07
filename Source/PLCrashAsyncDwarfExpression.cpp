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

extern "C" {
    #include "PLCrashAsyncDwarfExpression.h"
    #include <inttypes.h>
}

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @{
 */

template <typename machine_ptr> static plcrash_error_t plcrash_async_dwarf_eval_expression_int (plcrash_async_mobject_t *mobj,
                                                                                                const plcrash_async_byteorder_t *byteorder,
                                                                                                pl_vm_address_t start,
                                                                                                pl_vm_address_t end)
{
    
    //machine_ptr stack[255];
    //machine_ptr *sp = stack;

    return PLCRASH_ESUCCESS;
}

/**
 * Evaluate a DWARF expression, as defined in the DWARF 3 Specification, Section 2.5.
 *
 * @param mobj The memory object from which the expression opcodes will be read.
 * @param byteoder The byte order of the data referenced by @a mobj.
 * @param address_size The native address size of the target architecture. Currently, only 4 and 8
 * byte address widths are supported.
 * @param address The task-relative address within @a mobj at which the opcodes will be fetched.
 * @param offset An offset to be applied to @a address.
 * @param length The total length of the opcodes readable at @a address + @a offset.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an appropriate plcrash_error_t values
 * on failure.
 */
plcrash_error_t plcrash_async_dwarf_eval_expression (plcrash_async_mobject_t *mobj,
                                                     uint8_t address_size,
                                                     const plcrash_async_byteorder_t *byteorder,
                                                     pl_vm_address_t address,
                                                     pl_vm_off_t offset,
                                                     pl_vm_size_t length) {
    pl_vm_address_t start;
    pl_vm_address_t end;

    /* Calculate the terminating address */
    if (!plcrash_async_address_apply_offset(address, offset, &start)) {
        PLCF_DEBUG("Offset overflows base address");
        return PLCRASH_EINVAL;
    }
    
    if (length > PL_VM_OFF_MAX || !plcrash_async_address_apply_offset(start, length, &end)) {
        PLCF_DEBUG("Length overflows base address");
        return PLCRASH_EINVAL;
    }
    
    /* Call the appropriate implementation for the target's native pointer size */
    switch (address_size) {
        case 4:
            return plcrash_async_dwarf_eval_expression_int<uint32_t>(mobj, byteorder, start, end);
        case 8:
            return plcrash_async_dwarf_eval_expression_int<uint64_t>(mobj, byteorder, start, end);
        default:
            PLCF_DEBUG("Unsupported address size of %" PRIu8, address_size);
            return PLCRASH_EINVAL;
    }
}

/**
 * @}
 */
