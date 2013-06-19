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

#ifndef PLCRASH_ASYNC_DWARF_CFA_STATE_H
#define PLCRASH_ASYNC_DWARF_CFA_STATE_H 1

#include <cstddef>
#include <stdint.h>

extern "C" {
    #include "PLCrashAsync.h"
    #include "PLCrashAsyncDwarfPrimitives.h"
}

/**
 * @internal
 * @ingroup plcrash_async_dwarf_cfa_state
 * @{
 */

namespace plcrash {
    #define DWARF_CFA_STATE_MAX_STATES 6
    #define DWARF_CFA_STATE_BUCKET_COUNT 14
    #define DWARF_CFA_STATE_INVALID_ENTRY_IDX UINT8_MAX
    
    /* Consumes around 1.65k (on 32-bit and 64-bit systems). */
    #define DWARF_CFA_STATE_MAX_REGISTERS 100
    
    class dwarf_cfa_state_iterator;

    /** Call Frame Address type. */
    typedef enum {
        /** CFA is undefined. */
        DWARF_CFA_STATE_CFA_TYPE_UNDEFINED = 0,
        
        /** CFA is defined by a DWARF expression. */
        DWARF_CFA_STATE_CFA_TYPE_EXPRESSION = 1,
        
        /** CFA is defined by a register value + offset. */
        DWARF_CFA_STATE_CFA_TYPE_REGISTER = 2
    } dwarf_cfa_state_cfa_type_t;

    /**
     * @internal
     *
     * Manages a stack of CFA unwind register states, using sparsely allocated register column entries.
     *
     * Register numbers are sparsely allocated in the architecture-specific extensions to the DWARF spec,
     * requiring a solution other than allocating arrays large enough to hold the largest possible register number.
     * For example, ARM allocates or has set aside register values up to 8192, with 8192–16383 reserved for additional
     * vendor co-processor allocations.
     *
     * The actual total number of supported, active registers is much smaller. This class is built to decrease the
     * total amount of fixed stack space to be allocated; if we introduce our own async-safe heap allocator,
     * it may be reasonable to simplify this implementation to use the heap for entries.
     */
    class dwarf_cfa_state {
    private:
        /** A single register entry */
        typedef struct dwarf_cfa_reg_entry {
            /** Register value */
            int64_t value;

            /** The DWARF register number */
            uint32_t regnum;

            /** DWARF register rule */
            uint8_t rule;
            
            /** Next entry in the list, or NULL */
            uint8_t next;
        } dwarf_cfa_reg_entry_t;

        /** A CFA value rule.*/
        typedef struct dwarf_cfa_rule {
            /** The CFA type */
            dwarf_cfa_state_cfa_type_t cfa_type;
            
            union {
                /**
                 * CFA register value. (CFA = register + offset). Valid if type is DWARF_CFA_STATE_CFA_TYPE_REGISTER.
                 */
                struct {
                    /** CFA register */
                    uint32_t regnum;
                    
                    /** CFA register offset */
                    int32_t offset;
                } reg;
                
                /** CFA expression (CFA = expression). Valid if type is DWARF_CFA_STATE_CFA_TYPE_EXPRESSION. */
                int64_t expression;
            };
        } dwarf_cfa_rule_t;
        
        /** Current call frame value configuration. */
        dwarf_cfa_rule_t _cfa_value[DWARF_CFA_STATE_MAX_STATES];
        
        /** Current number of defined register entries */
        uint8_t _register_count[DWARF_CFA_STATE_MAX_STATES];

        /**
         * Active entry lookup table. Maps from regnum to a table index. Most architectures
         * define <20 valid register numbers.
         *
         * This provides a maximum of MAX_STATES saved states (DW_CFA_remember_state), with BUCKET_COUNT register
         * buckets available in each stack entry. Each bucket may hold multiple register entries; the
         * maximum number of register entries depends on the configured size of _entries.
         *
         * The pre-allocated entry set is shared between each saved state, as to decrease total
         * memory cost of unused states.
         */
        uint8_t _table_stack[DWARF_CFA_STATE_MAX_STATES][DWARF_CFA_STATE_BUCKET_COUNT];

        /** Current position in the table stack */
        uint8_t _table_depth;

        /** Free list of entries. These are unused records from the entries table. */
        uint8_t _free_list;

        /**
         * Statically allocated set of entries; these will be inserted into the free
         * list upon construction, and then moved into the entry table as registers
         * are set.
         */
        dwarf_cfa_reg_entry_t _entries[DWARF_CFA_STATE_MAX_REGISTERS];

    public:
        dwarf_cfa_state (void);
        bool set_register (uint32_t regnum, plcrash_dwarf_cfa_reg_rule_t rule, int64_t value);
        bool get_register_rule (uint32_t regnum, plcrash_dwarf_cfa_reg_rule_t *rule, int64_t *value);
        void remove_register (uint32_t regnum);
        uint8_t get_register_count (void);
        
        void set_cfa_register (uint32_t regnum, int32_t offset);
        void set_cfa_expression (int64_t expression);
        dwarf_cfa_rule_t get_cfa_rule (void);

        bool push_state (void);
        bool pop_state (void);
        
        friend class dwarf_cfa_state_iterator;
    };

    /**
     * A dwarf_cfa_state iterator; iterates DWARF CFA register records. The target stack must
     * not be modified while iteration is performed.
     */
    class dwarf_cfa_state_iterator {
    private:
        /** Current bucket index */
        uint8_t _bucket_idx;
        
        /** Current entry index, or DWARF_CFA_STATE_INVALID_ENTRY_IDX if iteration has not started */
        uint8_t _cur_entry_idx;
        
        /** Borrowed reference to the backing DWARF CFA state */
        dwarf_cfa_state *_stack;
        
    public:
        dwarf_cfa_state_iterator(dwarf_cfa_state *stack);
        bool next (uint32_t *regnum, plcrash_dwarf_cfa_reg_rule_t *rule, int64_t *value);
    };
  }

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_STACK_H */
