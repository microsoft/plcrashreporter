/*
 * Author: Mike Ash <mikeash@plausiblelabs.com>
 *
 * Copyright (c) 2012 Plausible Labs Cooperative, Inc.
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

#ifndef CrashReporter_PLCrashAsyncLocalSymbolication_h
#define CrashReporter_PLCrashAsyncLocalSymbolication_h

#include "PLCrashAsyncMachOImage.h"
#include "PLCrashAsyncObjCSection.h"


/**
 * Context object that helps repeated symbol lookups go faster.
 */
typedef struct pl_async_local_find_symbol_context {
    pl_async_objc_context_t objcContext;
} pl_async_local_find_symbol_context_t;

plcrash_error_t pl_async_local_find_symbol_context_init (pl_async_local_find_symbol_context_t *context);
void pl_async_local_find_symbol_context_free (pl_async_local_find_symbol_context_t *context);


/**
 * Prototype of a callback function used to execute user code with async-safely fetched symbol.
 *
 * @param address The symbol address.
 * @param name The symbol name. The callback is responsible for copying this value, as its backing storage is not gauranteed to exist
 * after the callback returns.
 * @param context The API client's supplied context value.
 */
typedef void (*pl_async_found_symbol_cb)(pl_vm_address_t address, const char *name, void *ctx);

plcrash_error_t pl_async_local_find_symbol(pl_async_macho_t *image, pl_async_local_find_symbol_context_t *findContext, pl_vm_address_t pc, pl_async_found_symbol_cb callback, void *ctx);

#endif
