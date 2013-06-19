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
    #define DWARF_CFA_STACK_MAX_STATES 6
    #define DWARF_CFA_STACK_BUCKET_COUNT 14
    #define DWARF_CFA_STACK_INVALID_ENTRY_IDX UINT8_MAX

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
    
    template <typename T, uint8_t S> class dwarf_cfa_stack_iterator;

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
        /** Current number of defined register entries */
        uint8_t _register_count[DWARF_CFA_STACK_MAX_STATES];

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
        uint8_t _table_stack[DWARF_CFA_STACK_MAX_STATES][DWARF_CFA_STACK_BUCKET_COUNT];

        /** Current position in the table stack */
        uint8_t _table_depth;

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
        bool set_register (uint32_t regnum, dwarf_cfa_reg_rule_t rule, T value);
        bool get_register_rule (uint32_t regnum, dwarf_cfa_reg_rule_t *rule, T *value);
        inline uint8_t get_register_count (void);
        
        friend class dwarf_cfa_stack_iterator<T,S>;
    };

    /**
     * A dwarf_cfa_stack iterator; iterates DWARF CFA register records. The target stack must
     * not be modified while iteration is performed.
     */
    template <typename T, uint8_t S> class dwarf_cfa_stack_iterator {
    private:
        /** Current bucket index */
        uint8_t _bucket_idx;
        
        /** Current entry index, or DWARF_CFA_STACK_INVALID_ENTRY_IDX if iteration has not started */
        uint8_t _cur_entry_idx;

        /** Borrowed reference to the backing DWARF CFA state */
        dwarf_cfa_stack<T,S> *_stack;
        
    public:
        dwarf_cfa_stack_iterator(dwarf_cfa_stack<T,S> *stack) {
            _stack = stack;
            _bucket_idx = 0;
            _cur_entry_idx = DWARF_CFA_STACK_INVALID_ENTRY_IDX;
        }

        bool next (uint32_t *regnum, dwarf_cfa_reg_rule_t *rule, T *value);
    };
    
    /*
     * Default constructor.
     */
    template <typename T, uint8_t S> dwarf_cfa_stack<T, S>::dwarf_cfa_stack (void) {
        /* The size must be smaller than the invalid entry index, which is used as a NULL flag */
        PLCF_ASSERT_STATIC(max_size, S < DWARF_CFA_STACK_INVALID_ENTRY_IDX);

        /* Initialize the free list */
        for (uint8_t i = 0; i < S; i++)
            _entries[i].next = i+1;

        /* Set the terminator flag on the last entry */
        _entries[S-1].next = DWARF_CFA_STACK_INVALID_ENTRY_IDX;

        /* First free entry is _entries[0] */
        _free_list = 0;
        
        /* Initial register count */
        plcrash_async_memset(_register_count, 0, sizeof(_register_count));
        
        /* Set up the table */
        _table_depth = 0;
        plcrash_async_memset(_table_stack, DWARF_CFA_STACK_INVALID_ENTRY_IDX, sizeof(_table_stack));
    }
    
    /**
     * Add a new register.
     *
     * @param regnum The DWARF register number.
     * @param rule The DWARF CFA rule for @a regnum.
     * @param value The data value to be used when interpreting @a rule.
     */
    template <typename T, uint8_t S> bool dwarf_cfa_stack<T, S>::set_register (uint32_t regnum, dwarf_cfa_reg_rule_t rule, T value) {
        PLCF_ASSERT(rule <= UINT8_MAX);

        /* Check for an existing entry, or find the target entry off which we'll chain our entry */
        unsigned int bucket = regnum % (sizeof(_table_stack[0]) / sizeof(_table_stack[0][0]));

        dwarf_cfa_reg_entry_t *parent = NULL;
        for (uint8_t parent_idx = _table_stack[_table_depth][bucket]; parent_idx != DWARF_CFA_STACK_INVALID_ENTRY_IDX; parent_idx = parent->next) {
            parent = &_entries[parent_idx];

            /* If an existing entry is found, we can re-use it directly */
            if (parent->regnum == regnum) {
                parent->value = value;
                parent->rule = rule;
                return true;
            }

            /* Otherwise, make sure we terminate with parent == last element */
            if (parent->next == DWARF_CFA_STACK_INVALID_ENTRY_IDX)
                break;
        }
        
        /* 'parent' now either points to the end of the list, or is NULL (in which case the table
         * slot was empty */
        dwarf_cfa_reg_entry *entry = NULL;
        uint8_t entry_idx;

        /* Fetch a free entry */
        if (_free_list == DWARF_CFA_STACK_INVALID_ENTRY_IDX) {
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
        entry->next = DWARF_CFA_STACK_INVALID_ENTRY_IDX;
        
        /* Either insert in the parent, or insert as the first table element */
        if (parent == NULL) {
            _table_stack[_table_depth][bucket] = entry_idx;
        } else {
            parent->next = entry - _entries;
        }

        _register_count[_table_depth]++;
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
        for (uint8_t entry_idx = _table_stack[_table_depth][bucket]; entry_idx != DWARF_CFA_STACK_INVALID_ENTRY_IDX; entry_idx = entry->next) {
            entry = &_entries[entry_idx];

            if (entry->regnum != regnum) {
                if (entry->next == DWARF_CFA_STACK_INVALID_ENTRY_IDX)
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
    
    /**
     * Return the number of register rules set for the current register state.
     */
    template <typename T, uint8_t S> inline uint8_t dwarf_cfa_stack<T,S>::get_register_count (void) {
        return _register_count[_table_depth];
    }

    /**
     * Enumerate the next register entry. Returns true on success, or false if no additional entries are available.
     *
     * @param regnum[out] On success, the DWARF register number.
     * @param rule[out] On success, the DWARF CFA rule for @a regnum.
     * @param value[out] On success, the data value to be used when interpreting @a rule.
     */
    template <typename T, uint8_t S> bool dwarf_cfa_stack_iterator<T,S>::next (uint32_t *regnum, dwarf_cfa_reg_rule_t *rule, T *value) {
        /* Fetch the next entry in the bucket chain */
        if (_cur_entry_idx != DWARF_CFA_STACK_INVALID_ENTRY_IDX) {
            _cur_entry_idx = _stack->_entries[_cur_entry_idx].next;
            
            /* Advance to the next bucket if we've reached the end of the current chain */
            if (_cur_entry_idx == DWARF_CFA_STACK_INVALID_ENTRY_IDX)
                _bucket_idx++;
        }

        /*
         * On the first iteration, or after the end of a bucket chain has been reached, find the next valid bucket chain.
         * Otherwise, we have a valid bucket chain and simply need the next entry.
         */
        if (_cur_entry_idx == DWARF_CFA_STACK_INVALID_ENTRY_IDX) {
            for (; _bucket_idx < DWARF_CFA_STACK_BUCKET_COUNT; _bucket_idx++) {
                if (_stack->_table_stack[_stack->_table_depth][_bucket_idx] != DWARF_CFA_STACK_INVALID_ENTRY_IDX) {
                    _cur_entry_idx = _stack->_table_stack[_stack->_table_depth][_bucket_idx];
                    break;
                }
            }

            /* If we get here without a valid entry, we've hit the end of all bucket chains. */
            if (_cur_entry_idx == DWARF_CFA_STACK_INVALID_ENTRY_IDX)
                return false;
        }


        typename dwarf_cfa_stack<T,S>::dwarf_cfa_reg_entry_t *entry = &_stack->_entries[_cur_entry_idx];
        *regnum = entry->regnum;
        *value = entry->value;
        *rule = (dwarf_cfa_reg_rule_t) entry->rule;
        return true;
    }
}

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_STACK_H */
