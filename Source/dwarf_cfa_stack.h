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

#ifndef PLCRASH_ASYNC_DWARF_CFA_STACK_H
#define PLCRASH_ASYNC_DWARF_CFA_STACK_H 1

#include <cstddef>
#include <stdint.h>

extern "C" {
#include "PLCrashAsync.h"
}

/**
 * @internal
 * @ingroup plcrash_async_dwarf_cfa_stack
 * @{
 */

namespace plcrash {
    /**
     * Register rules, as defined in DWARF 4 Section 6.4.1.
     */
    typedef enum {
        /**
         * The previous value of this register is saved at the address CFA+N where CFA is the current
         * CFA value and N is a signed offset.
         */
        DWARF_CFA_REG_RULE_OFFSET,
    } dwarf_cfa_reg_rule_t;

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
    template <typename T, uint8_t S> class dwarf_cfa_stack {
    public:
        /** A single register entry */
        typedef struct dwarf_cfa_reg_entry {
            /** Register value */
            T value;

            /** The DWARF register number */
            uint32_t regnum;

            /** DWARF register rule */
            uint8_t rule;
            
            /** Next entry in the list, or NULL */
            uint8_t next;
        } dwarf_cfa_reg_entry_t;
        
    private:
        /**
         * Active entry lookup table. Maps from regnum to a table index. Most architectures
         * define <20 valid register numbers.
         *
         * This provides a maximum of 6 saved states (DW_CFA_remember_state), with 15 register buckets
         * available in each row. Each bucket may hold multiple register entries; the
         * maximum number of register entries depends on the configured size of _entries.
         *
         * The pre-allocated entry set is shared between each saved state, as to decrease total
         * memory cost of unused states.
         */
        uint8_t _table_stack[6][15];

        /** Current position in the table stack */
        uint8_t _table_pos;

        /** Free list of entries. These are unused records from the entries table. */
        uint8_t _free_list;

        /**
         * Statically allocated set of entries; these will be inserted into the free
         * list upon construction, and then moved into the entry table as registers
         * are set.
         */
        dwarf_cfa_reg_entry_t _entries[S];

    public:
        dwarf_cfa_stack (void);
        bool add_register (uint32_t regnum, dwarf_cfa_reg_rule_t rule, T value);
        bool get_register_rule (uint32_t regnum, dwarf_cfa_reg_rule_t *rule, T *value);
    };
    
    
    /*
     * Default constructor.
     */
    template <typename T, uint8_t S> dwarf_cfa_stack<T, S>::dwarf_cfa_stack (void) {
        /* The size must be smaller than UINT8_MAX, which is used as a NULL flag */
        PLCF_ASSERT_STATIC(max_size, S < UINT8_MAX);

        /* Initialize the free list */
        for (uint8_t i = 0; i < S; i++)
            _entries[i].next = i+1;
        
        _entries[S-1].next = UINT8_MAX;
        _free_list = 0;
        
        /* Set up the table */
        _table_pos = 0;
        plcrash_async_memset(_table_stack, UINT8_MAX, sizeof(_table_stack));
    }
    
    /**
     * Add a new register for the current row.
     *
     * @param regnum The DWARF register number.
     * @param rule The DWARF CFA rule for @a regnum.
     * @param value The data value to be used when interpreting @a rule.
     */
    template <typename T, uint8_t S> bool dwarf_cfa_stack<T, S>::add_register (uint32_t regnum, dwarf_cfa_reg_rule_t rule, T value) {
        PLCF_ASSERT(rule <= UINT8_MAX);

        /* Check for an existing entry, or find the target entry off which we'll chain our entry */
        unsigned int bucket = regnum % (sizeof(_table_stack[0]) / sizeof(_table_stack[0][0]));

        dwarf_cfa_reg_entry_t *parent = NULL;
        for (uint8_t parent_idx = _table_stack[_table_pos][bucket]; parent_idx != UINT8_MAX; parent_idx = parent->next) {
            parent = &_entries[parent_idx];

            /* If an existing entry is found, we can re-use it directly */
            if (parent->regnum == regnum) {
                parent->value = value;
                parent->rule = rule;
                return true;
            }

            /* Otherwise, make sure we terminate with parent == last element */
            if (parent->next == UINT8_MAX)
                break;
        }
        
        /* 'parent' now either points to the end of the list, or is NULL (in which case the table
         * slot was empty */
        dwarf_cfa_reg_entry *entry = NULL;
        uint8_t entry_idx;

        /* Fetch a free entry */
        if (_free_list == UINT8_MAX) {
            /* No free entries */
            return false;
        }
        entry_idx = _free_list;
        entry = &_entries[entry_idx];
        _free_list = entry->next;

        /* Intialize the entry */
        entry->regnum = regnum;
        entry->rule = rule;
        entry->value = value;
        entry->next = UINT8_MAX;
        
        /* Either insert in the parent, or insert as the first table element */
        if (parent == NULL) {
            _table_stack[_table_pos][bucket] = entry_idx;
        } else {
            parent->next = entry - _entries;
        }
        
        return true;
    }

    /**
     * Fetch the register entry data for a given DWARF register number, returning
     * true on success, or false if no entry has been added for the register.
     *
     * @param regnum The DWARF register number.
     * @param rule[out] On success, the DWARF CFA rule for @a regnum.
     * @param value[out] On success, the data value to be used when interpreting @a rule.
     */
    template <typename T, uint8_t S> bool dwarf_cfa_stack<T,S>::get_register_rule (uint32_t regnum, dwarf_cfa_reg_rule_t *rule, T *value) {
        /* Search for the entry */
        unsigned int bucket = regnum % (sizeof(_table_stack[0]) / sizeof(_table_stack[0][0]));
    
        dwarf_cfa_reg_entry_t *entry = NULL;
        for (uint8_t entry_idx = _table_stack[_table_pos][bucket]; entry_idx != UINT8_MAX; entry_idx = entry->next) {
            entry = &_entries[entry_idx];

            if (entry->regnum != regnum) {
                if (entry->next == UINT8_MAX)
                    break;

                continue;
            }
            
            /* Existing entry found, we can re-use it directly */
            *value = entry->value;
            *rule = (dwarf_cfa_reg_rule_t) entry->rule;
            return true;
        }

        /* Not found? */
        return false;
    }
}

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_STACK_H */
