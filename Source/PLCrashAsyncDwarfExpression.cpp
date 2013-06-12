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
    #include "PLCrashAsyncDwarfPrimitives.h"
    #include <inttypes.h>
}

#include "dwarf_stack.h"

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @{
 */

/**
 * Read a value of type and size @a T from @a *p, advancing @a p the past the read value,
 * and verifying that the read will not overrun the address @a maxpos.
 *
 * @param p The pointer from which the value will be read. This pointer will be advanced by sizeof(T).
 * @param maxpos The maximum address from which a value may be read.
 * @param result The destination to which the result will be written.
 *
 * @return Returns true on success, or false if the read would exceed the boundry specified by @a maxpos.
 */
template <typename T> static inline bool dw_expr_read_impl (void **p, void *maxpos, T *result) {
    if ((uint8_t *)maxpos - (uint8_t *)*p < sizeof(T)) {
        return false;
    }
    
    *result = *((T *)*p);
    *((uint8_t **)p) += sizeof(T);
    return true;
}

/**
 * Evaluate the expression opcodes starting at address expression evaluation imlementation.
 */

/**
 * Evaluate a DWARF expression, as defined in the DWARF 3 Specification, Section 2.5. This
 * internal implementation is templated to support 32-bit and 64-bit evaluation.
 *
 * @param mobj The memory object from which the expression opcodes will be read.
 * @param task The task from which any DWARF expression memory loads will be performed.
 * @param thread_state The thread state against which the expression will be evaluated.
 * @param byteoder The byte order of the data referenced by @a mobj and @a thread_state.
 * @param address The task-relative address within @a mobj at which the opcodes will be fetched.
 * @param offset An offset to be applied to @a address.
 * @param length The total length of the opcodes readable at @a address + @a offset.
 * @param result[out] On success, the evaluation result. As per DWARF 3 section 2.5.1, this will be
 * the top-most element on the evaluation stack. If the stack is empty, an error will be returned
 * and no value will be written to this parameter.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an appropriate plcrash_error_t values
 * on failure. If an invalid opcode is detected, PLCRASH_ENOTSUP will be returned. If the stack
 * is empty upon termination of evaluation, PLCRASH_EINVAL will be returned.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an appropriate plcrash_error_t values
 * on failure.
 *
 * @todo Consider defining updated status codes or error handling to provide more structured
 * error data on failure.
 */
template <typename machine_ptr, typename machine_ptr_s>
static plcrash_error_t plcrash_async_dwarf_eval_expression_int (plcrash_async_mobject_t *mobj,
                                                                task_t task,
                                                                plcrash_async_thread_state_t *thread_state,
                                                                const plcrash_async_byteorder_t *byteorder,
                                                                pl_vm_address_t address,
                                                                pl_vm_off_t offset,
                                                                pl_vm_size_t length,
                                                                machine_ptr *result)
{
    // TODO: Review the use of an up-to-800 byte stack allocation; we may want to replace this with
    // use of the new async-safe allocator.
    plcrash::dwarf_stack<machine_ptr, 100> stack;
    plcrash_error_t err;

    pl_vm_address_t start;
    pl_vm_address_t end;

    /* Calculate the start and end addresses */
    if (!plcrash_async_address_apply_offset(address, offset, &start)) {
        PLCF_DEBUG("Offset overflows base address");
        return PLCRASH_EINVAL;
    }
    
    if (length > PL_VM_OFF_MAX || !plcrash_async_address_apply_offset(start, length, &end)) {
        PLCF_DEBUG("Length overflows base address");
        return PLCRASH_EINVAL;
    }

    /* Map in the full instruction range */
    void *instr = plcrash_async_mobject_remap_address(mobj, start, 0, end-start);
    void *instr_max = (uint8_t *)instr + (end - start);

    if (instr == NULL) {
        PLCF_DEBUG("Could not map the DWARF instructions; range falls outside mapped pages");
        return PLCRASH_EINVAL;
    }
    
    /*
     * Note that the below value macros all cast data to the appropriate target machine word size.
     * This will result in overflows, as defined in the DWARF specification; the unsigned overflow
     * behavior is defined, and as per DWARF and C, the signed overflow behavior is not.
     */
    
    /* A position-advancing read macro that uses GCC/clang's compound statement value extension, returning PLCRASH_EINVAL
     * if the read extends beyond the mapped range. */
#define dw_expr_read(_type) ({ \
    _type v; \
    if (!dw_expr_read_impl<_type>(&p, instr_max, &v)) { \
        PLCF_DEBUG("Read of size %zu exceeds mapped range", sizeof(v)); \
        return PLCRASH_EINVAL; \
    } \
    v; \
})
    
    /* A position-advancing uleb128 read macro that uses GCC/clang's compound statement value extension, returning an error
     * if the read fails. */
#define dw_expr_read_uleb128() ({ \
    plcrash_error_t err; \
    uint64_t v; \
    pl_vm_size_t lebsize; \
    pl_vm_off_t offset = ((uint8_t *)p - (uint8_t *)instr); \
    \
    if ((err = plcrash_async_dwarf_read_uleb128(mobj, start, offset, &v, &lebsize)) != PLCRASH_ESUCCESS) { \
        PLCF_DEBUG("Read of ULEB128 value failed"); \
        return err; \
    } \
    \
    p = (uint8_t *)p + lebsize; \
    (machine_ptr) v; \
})

    /* A position-advancing sleb128 read macro that uses GCC/clang's compound statement value extension, returning an error
     * if the read fails. */
#define dw_expr_read_sleb128() ({ \
    plcrash_error_t err; \
    int64_t v; \
    pl_vm_size_t lebsize; \
    pl_vm_off_t offset = ((uint8_t *)p - (uint8_t *)instr); \
    \
    if ((err = plcrash_async_dwarf_read_sleb128(mobj, start, offset, &v, &lebsize)) != PLCRASH_ESUCCESS) { \
        PLCF_DEBUG("Read of SLEB128 value failed"); \
        return err; \
    } \
    \
    p = (uint8_t *)p + lebsize; \
    (machine_ptr_s) v; \
})
    
    /* Macro to fetch register valeus; handles unsupported register numbers and missing registers values */
#define dw_thread_regval(_dw_regnum) ({ \
    plcrash_regnum_t rn = plcrash_async_thread_state_map_dwarf_reg(thread_state, _dw_regnum); \
    if (rn == PLCRASH_REG_INVALID) { \
        PLCF_DEBUG("Unsupported DWARF register value of 0x%" PRIx64, (uint64_t)_dw_regnum);\
        return PLCRASH_ENOTSUP; \
    } \
\
    if (!plcrash_async_thread_state_has_reg(thread_state, rn)) { \
        PLCF_DEBUG("Register value of %s unavailable in the current frame.", plcrash_async_thread_state_get_reg_name(thread_state, rn)); \
        return PLCRASH_ENOTFOUND; \
    } \
\
    plcrash_greg_t val = plcrash_async_thread_state_get_reg(thread_state, rn); \
    (machine_ptr) val; \
})

    /* A push macro that handles reporting of stack overflow errors */
#define dw_expr_push(v) if (!stack.push(v)) { \
    PLCF_DEBUG("Hit stack limit; cannot push further values"); \
    return PLCRASH_EINTERNAL; \
}
    
    /* A pop macro that handles reporting of stack underflow errors */
#define dw_expr_pop(v) if (!stack.pop(v)) { \
    PLCF_DEBUG("Pop on an empty stack"); \
    return PLCRASH_EINTERNAL; \
}

    void *p = instr;
    while (p < instr_max) {
        uint8_t opcode = dw_expr_read(uint8_t);

        switch (opcode) {
            case DW_OP_lit0:
            case DW_OP_lit1:
            case DW_OP_lit2:
            case DW_OP_lit3:
            case DW_OP_lit4:
            case DW_OP_lit5:
            case DW_OP_lit6:
            case DW_OP_lit7:
            case DW_OP_lit8:
            case DW_OP_lit9:
            case DW_OP_lit10:
            case DW_OP_lit11:
            case DW_OP_lit12:
            case DW_OP_lit13:
            case DW_OP_lit14:
            case DW_OP_lit15:
            case DW_OP_lit16:
            case DW_OP_lit17:
            case DW_OP_lit18:
            case DW_OP_lit19:
            case DW_OP_lit20:
            case DW_OP_lit21:
            case DW_OP_lit22:
            case DW_OP_lit23:
            case DW_OP_lit24:
            case DW_OP_lit25:
            case DW_OP_lit26:
            case DW_OP_lit27:
            case DW_OP_lit28:
            case DW_OP_lit29:
            case DW_OP_lit30:
            case DW_OP_lit31:
                dw_expr_push(opcode-DW_OP_lit0);
                break;
                
            case DW_OP_const1u:
                dw_expr_push(dw_expr_read(uint8_t));
                break;

            case DW_OP_const1s:
                dw_expr_push(dw_expr_read(int8_t));
                break;
                
            case DW_OP_const2u:
                dw_expr_push(byteorder->swap16(dw_expr_read(uint16_t)));
                break;
                
            case DW_OP_const2s:
                dw_expr_push((int16_t) byteorder->swap16(dw_expr_read(int16_t)));
                break;
                
            case DW_OP_const4u:
                dw_expr_push(byteorder->swap32(dw_expr_read(uint32_t)));
                break;
                
            case DW_OP_const4s:
                dw_expr_push((int32_t) byteorder->swap32(dw_expr_read(int32_t)));
                break;
                
            case DW_OP_const8u:
                dw_expr_push(byteorder->swap64(dw_expr_read(uint64_t)));
                break;
                
            case DW_OP_const8s:
                dw_expr_push((int64_t) byteorder->swap64(dw_expr_read(int64_t)));
                break;
                
            case DW_OP_constu:
                dw_expr_push(dw_expr_read_uleb128());
                break;
                
            case DW_OP_consts:
                dw_expr_push(dw_expr_read_sleb128());
                break;
                
            case DW_OP_breg0:
			case DW_OP_breg1:
			case DW_OP_breg2:
			case DW_OP_breg3:
			case DW_OP_breg4:
			case DW_OP_breg5:
			case DW_OP_breg6:
			case DW_OP_breg7:
			case DW_OP_breg8:
			case DW_OP_breg9:
			case DW_OP_breg10:
			case DW_OP_breg11:
			case DW_OP_breg12:
			case DW_OP_breg13:
			case DW_OP_breg14:
			case DW_OP_breg15:
			case DW_OP_breg16:
			case DW_OP_breg17:
			case DW_OP_breg18:
			case DW_OP_breg19:
			case DW_OP_breg20:
			case DW_OP_breg21:
			case DW_OP_breg22:
			case DW_OP_breg23:
			case DW_OP_breg24:
			case DW_OP_breg25:
			case DW_OP_breg26:
			case DW_OP_breg27:
			case DW_OP_breg28:
			case DW_OP_breg29:
			case DW_OP_breg30:
			case DW_OP_breg31:
                dw_expr_push(dw_thread_regval(opcode - DW_OP_breg0) + dw_expr_read_sleb128());
                break;
                
            case DW_OP_bregx:
                dw_expr_push(dw_thread_regval(dw_expr_read_sleb128()) + dw_expr_read_sleb128());
                
            case DW_OP_dup:
                if (!stack.dup()) {
                    PLCF_DEBUG("DW_OP_dup on an empty stack");
                    return PLCRASH_EINVAL;
                }
                break;
                
            case DW_OP_drop: {
                if (!stack.drop()) {
                    PLCF_DEBUG("DW_OP_drop on an empty stack");
                    return PLCRASH_EINVAL;
                }
                break;
            }
                
            case DW_OP_pick:
                if (!stack.pick(dw_expr_read(uint8_t))) {
                    PLCF_DEBUG("DW_OP_pick on invalid index");
                    return PLCRASH_EINVAL;
                }
                break;

            case DW_OP_over:
                if (!stack.pick(1)) {
                    PLCF_DEBUG("DW_OP_over on stack with < 2 elements");
                    return PLCRASH_EINVAL;
                }
                break;
                
            case DW_OP_swap:
                if (!stack.swap()) {
                    PLCF_DEBUG("DW_OP_swap on stack with < 2 elements");
                    return PLCRASH_EINVAL;
                }
                break;
                
            case DW_OP_rot:
                if (!stack.rotate()) {
                    PLCF_DEBUG("DW_OP_rot on stack with < 3 elements");
                    return PLCRASH_EINVAL;
                }
                break;
            
                
            case DW_OP_xderef:
                /* This is identical to deref, except that it consumes an additional stack value
                 * containing the address space of the address. We don't support any systems with multiple
                 * address spaces, so we simply excise this value from the stack and fall through to the
                 * deref implementation */

                /* Move the address space value to the top of the stack, and then drop it */
                if (!stack.swap()) {
                    PLCF_DEBUG("DW_OP_xderef on stack with < 2 elements");
                    return PLCRASH_EINVAL;
                }
                
                /* This can't fail after the swap suceeded */
                stack.drop();
                
            case DW_OP_deref: {
                machine_ptr addr;
                machine_ptr value;

                dw_expr_pop(&addr);
                if ((err = plcrash_async_safe_memcpy(task, addr, 0, &value, sizeof(value))) != PLCRASH_ESUCCESS) {
                    PLCF_DEBUG("DW_OP_deref referenced an invalid target address 0x%" PRIx64, (uint64_t) addr);
                    return err;
                }

                dw_expr_push(value);
                
                break;
            }
            case DW_OP_xderef_size:
                /* This is identical to deref_size, except that it consumes an additional stack value
                 * containing the address space of the address. We don't support any systems with multiple
                 * address spaces, so we simply excise this value from the stack and fall through to the
                 * deref implementation */
                
                /* Move the address space value to the top of the stack, and then drop it */
                if (!stack.swap()) {
                    PLCF_DEBUG("DW_OP_xderef_size on stack with < 2 elements");
                    return PLCRASH_EINVAL;
                }

                /* This can't fail after the swap suceeded */
                stack.drop();

            case DW_OP_deref_size: {
                /* Fetch the target size */
                uint8_t size = dw_expr_read(uint8_t);
                if (size > sizeof(machine_ptr)) {
                    PLCF_DEBUG("DW_OP_deref_size specified a size larger than the native machine word");
                    return PLCRASH_EINVAL;
                }
                
                /* Pop the address from the stack */
                machine_ptr addr;
                dw_expr_pop(&addr);

                /* Perform the read */
                #define readval(_type) case sizeof(_type): { \
                    _type r; \
                    if ((err = plcrash_async_safe_memcpy(task, (pl_vm_address_t)addr, 0, &r, sizeof(_type))) != PLCRASH_ESUCCESS) { \
                        PLCF_DEBUG("DW_OP_deref_size referenced an invalid target address 0x%" PRIx64, (uint64_t) addr); \
                        return err; \
                    } \
                    value = r; \
                    break; \
                }
                machine_ptr value = 0;
                switch (size) {
                    readval(uint8_t);
                    readval(uint16_t);
                    readval(uint32_t);
                    readval(uint64_t);

                    default:
                        PLCF_DEBUG("DW_OP_deref_size specified an unsupported size of %"PRIu8, size);
                        return PLCRASH_EINVAL;
                }
                #undef readval

                dw_expr_push(value);
                
                break;
            }
                
            case DW_OP_abs: {
                machine_ptr_s v;
                dw_expr_pop((machine_ptr *)&v);
                if (v < 0) {
                    dw_expr_push(-v);
                } else {
                    dw_expr_push(v);
                }
                break;
            }
                
            case DW_OP_and: {
                machine_ptr v1, v2;
                dw_expr_pop(&v1);
                dw_expr_pop(&v2);                
                dw_expr_push(v1 & v2);
                break;
            }
                
            case DW_OP_div: {
                machine_ptr_s divisor;
                machine_ptr dividend;
                
                dw_expr_pop((machine_ptr *) &divisor);
                dw_expr_pop(&dividend);
                
                if (divisor == 0) {
                    PLCF_DEBUG("DW_OP_div attempted divide by zero");
                    return PLCRASH_EINVAL;
                }
                
                machine_ptr result = dividend / divisor;
                dw_expr_push(result);
                break;
            }

            case DW_OP_nop: // no-op
                break;
                
            // Not implemented -- fall through
            case DW_OP_fbreg:
                /* Unimplemented */

            case DW_OP_push_object_address:
                /*
                 * As per DWARF 3, Section 6.4.2 Call Frame Instructions, DW_OP_push_object_address is not meaningful in an operand of these
                 * instructions because there is no object context to provide a value to push.
                 *
                 * If this implementation is further extended for use outside of CFI evaluation, this opcode should be implemented.
                 */

            case DW_OP_form_tls_address:
                /* The structure of TLS data on Darwin is implementation private. */
                
            case DW_OP_call_frame_cfa:
                /*
                 * As per DWARF 3, Section 6.4.2 Call Frame Instructions, DW_OP_call_frame_cfa is not meaningful in an operand of these
                 * instructions because its use would be circular.
                 *
                 * If this implementation is further extended for use outside of CFI evaluation, this opcode should be implemented.
                 */
                
            default:
                PLCF_DEBUG("Unsupported opcode 0x%" PRIx8, opcode);
                return PLCRASH_ENOTSUP;
        }
    }

    /* Provide the result */
    if (!stack.pop(result)) {
        PLCF_DEBUG("Expression did not provide a result value.");
        return PLCRASH_EINVAL;
    }

#undef dw_expr_read
#undef dw_expr_push
    return PLCRASH_ESUCCESS;
}

/**
 * Evaluate a DWARF expression, as defined in the DWARF 3 Specification, Section 2.5,
 * using a 32-bit stack.
 *
 * @param mobj The memory object from which the expression opcodes will be read.
 * @param task The task from which any DWARF expression memory loads will be performed.
 * @param thread_state The thread state against which the expression will be evaluated.
 * @param byteoder The byte order of the data referenced by @a mobj and @a thread_state.
 * @param address The task-relative address within @a mobj at which the opcodes will be fetched.
 * @param offset An offset to be applied to @a address.
 * @param length The total length of the opcodes readable at @a address + @a offset.
 * @param result[out] On success, the evaluation result. As per DWARF 3 section 2.5.1, this will be
 * the top-most element on the evaluation stack. If the stack is empty, an error will be returned
 * and no value will be written to this parameter.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an appropriate plcrash_error_t values
 * on failure. If an invalid opcode is detected, PLCRASH_ENOTSUP will be returned. If the stack
 * is empty upon termination of evaluation, PLCRASH_EINVAL will be returned.
 */
plcrash_error_t plcrash_async_dwarf_eval_expression_32 (plcrash_async_mobject_t *mobj,
                                                        task_t task,
                                                        plcrash_async_thread_state_t *thread_state,
                                                        const plcrash_async_byteorder_t *byteorder,
                                                        pl_vm_address_t address,
                                                        pl_vm_off_t offset,
                                                        pl_vm_size_t length,
                                                        uint32_t *result)
{
    plcrash_error_t err;

    uint32_t v;
    if ((err = plcrash_async_dwarf_eval_expression_int<uint32_t, int32_t>(mobj, task, thread_state, byteorder, address, offset, length, &v)) == PLCRASH_ESUCCESS)
        *result = v;

    return err;
}

/**
 * Evaluate a DWARF expression, as defined in the DWARF 3 Specification, Section 2.5,
 * using a 64-bit stack.
 *
 * @param mobj The memory object from which the expression opcodes will be read.
 * @param task The task from which any DWARF expression memory loads will be performed.
 * @param thread_state The thread state against which the expression will be evaluated.
 * @param byteoder The byte order of the data referenced by @a mobj and @a thread_state.
 * @param address The task-relative address within @a mobj at which the opcodes will be fetched.
 * @param offset An offset to be applied to @a address.
 * @param length The total length of the opcodes readable at @a address + @a offset.
 * @param result[out] On success, the evaluation result. As per DWARF 3 section 2.5.1, this will be
 * the top-most element on the evaluation stack. If the stack is empty, an error will be returned
 * and no value will be written to this parameter.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an appropriate plcrash_error_t values
 * on failure. If an invalid opcode is detected, PLCRASH_ENOTSUP will be returned. If the stack
 * is empty upon termination of evaluation, PLCRASH_EINVAL will be returned.
 */
plcrash_error_t plcrash_async_dwarf_eval_expression_64 (plcrash_async_mobject_t *mobj,
                                                        task_t task,
                                                        plcrash_async_thread_state_t *thread_state,
                                                        const plcrash_async_byteorder_t *byteorder,
                                                        pl_vm_address_t address,
                                                        pl_vm_off_t offset,
                                                        pl_vm_size_t length,
                                                        uint64_t *result)
{
    plcrash_error_t err;
    
    uint64_t v;
    if ((err = plcrash_async_dwarf_eval_expression_int<uint64_t, int64_t>(mobj, task, thread_state, byteorder, address, offset, length, &v)) == PLCRASH_ESUCCESS)
        *result = v;

    return err;
}

/**
 * @}
 */
