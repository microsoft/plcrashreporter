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

#ifndef PLCRASH_ASYNC_DWARF_LSDA_H
#define PLCRASH_ASYNC_DWARF_LSDA_H

#include "PLCrashAsync.h"
#include "PLCrashAsyncMObject.h"
#include "PLCrashAsyncDwarfPrimitives.hpp"
#include "PLCrashFeatureConfig.h"
#include <vector>

#if PLCRASH_FEATURE_UNWIND_DWARF

namespace plcrash {

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @{
 */

struct dwarf_lsda_callsite_info;
struct dwarf_lsda_action_record;

/**
 * A DWARF Language Specific Data Area (LSDA) record. This describes how to
 * handle exceptions within a function.
 */
typedef struct dwarf_lsda_info {
    /** Is the lp_start field present in the LSDA? */
    bool has_lp_start;
    /**
     * The address that all landing pads described by the LSDA are relative to.
     * If this field is absent, landing pads are relative to the beginning of
     * the function.
     */
    uintptr_t lp_start;

    std::vector<dwarf_lsda_callsite_info> callsites;
} dwarf_lsda_info_t;

/**
 * A DWARF exception handling callsite record. This represents a range of code
 * in a function and describes where to jump if an exception is raised in that
 * range.
 */
typedef struct dwarf_lsda_callsite_info {
    /** The byte offset from the start of the function for this range. */
    pl_vm_address_t start;
    /** The number of bytes this range covers. */
    pl_vm_off_t length;
    /**
     * The offset to the landing pad in bytes, measured from the start of the
     * function or, if specified, the lp_start field in the lsda header.
     */
    pl_vm_off_t landing_pad_offset;
    /**
     * A collection of actions that the personality function will use to
     * dispatch the exception or perform validation.
     */
    std::vector<dwarf_lsda_action_record> actions;
} dwarf_lsda_callsite_info_t;

typedef enum {
    DWARF_LSDA_ACTION_KIND_CATCH,
    DWARF_LSDA_ACTION_KIND_EXCEPTION_SPECIFICATION
} dwarf_lsda_action_kind_t;

/**
 * A DWARF Exception handling action record. This describes either a type that
 * can be caught or a set of types that is allowed to be thrown.
 */
typedef struct dwarf_lsda_action_record {
    dwarf_lsda_action_kind_t kind;
    /**
     * The type information that this action matches against. For catch actions,
     * this is the type that can be caught and will have one item. For
     * exception specifications, this will be all of the types that the code is
     * allowed to throw.
     */
    std::vector<pl_vm_address_t> types;
} dwarf_lsda_action_record_t;

template <typename machine_ptr>
plcrash_error_t dwarf_lsda_info_init (dwarf_lsda_info_t *info,
                                      plcrash_async_mobject_t *mobj,
                                      const plcrash_async_byteorder_t *byteorder,
                                      pl_vm_address_t address);

void dwarf_lsda_info_free (dwarf_lsda_info_t *info);

/**
 * @}
 */

}

#endif
#endif /* defined(PLCRASH_ASYNC_DWARF_LSDA_H) */
