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

#ifndef PLCRASH_ASYNC_DWARF_EXPRESSION_H
#define PLCRASH_ASYNC_DWARF_EXPRESSION_H 1

#include "PLCrashAsync.h"
#include "PLCrashAsyncMObject.h"

/**
 * @internal
 * @ingroup plcrash_async_dwarf
 * @{
 */

plcrash_error_t plcrash_async_dwarf_eval_expression (plcrash_async_mobject_t *mobj,
                                                     uint8_t address_size,
                                                     const plcrash_async_byteorder_t *byteorder,
                                                     pl_vm_address_t address,
                                                     pl_vm_off_t offset,
                                                     pl_vm_size_t length);



/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_CFA_H */
