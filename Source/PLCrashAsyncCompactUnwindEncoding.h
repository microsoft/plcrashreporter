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

#ifndef PLCRASH_ASYNC_COMPACT_UNWIND_ENCODING_H
#define PLCRASH_ASYNC_COMPACT_UNWIND_ENCODING_H 1

#include "PLCrashAsync.h"
#include "PLCrashAsyncImageList.h"

/**
 * @internal
 * @ingroup plcrash_async_cfe
 * @{
 */

/**
 * A CFE reader instance. Performs CFE data parsing from a backing memory object.
 */
typedef struct plcrash_async_cfe_reader {
    /** A memory object containing the CFE data at the starting address. */
    plcrash_async_mobject_t *mobj;
} plcrash_async_cfe_reader_t;

plcrash_error_t plcrash_async_cfe_reader_init (plcrash_async_cfe_reader_t *cfe, plcrash_async_mobject_t *mobj);
void plcrash_async_cfe_reader_free (plcrash_async_cfe_reader_t *cfe);

/**
 * @} plcrash_async_cfe
 */

#endif /* PLCRASH_ASYNC_COMPACT_UNWIND_ENCODING_H */