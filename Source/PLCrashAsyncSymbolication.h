/*
 * Author: Mike Ash <mikeash@plausiblelabs.com>
 *
 * Copyright (c) 2012-2013 Plausible Labs Cooperative, Inc.
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
 * Context object that helps speed up repeated symbol lookups.
 *
 * @warning It is invalid to reuse this context for multiple Mach tasks.
 * @warning Any plcrash_async_macho_t pointers passed in must be valid across all
 * calls using this context.
 */
typedef struct plcrash_async_symbol_cache {
    /** Objective-C look-up cache. */
    plcrash_async_objc_cache_t objc_cache;
} plcrash_async_symbol_cache_t;

plcrash_error_t plcrash_async_symbol_cache_init (plcrash_async_symbol_cache_t *cache);
void plcrash_async_symbol_cache_free (plcrash_async_symbol_cache_t *cache);


/**
 * Prototype of a callback function used to execute user code with async-safely fetched symbol.
 *
 * @param address The symbol address.
 * @param name The symbol name. The callback is responsible for copying this value, as its backing storage is not gauranteed to exist
 * after the callback returns.
 * @param context The API client's supplied context value.
 */
typedef void (*plcrash_async_found_symbol_cb)(pl_vm_address_t address, const char *name, void *ctx);

plcrash_error_t plcrash_async_find_symbol(plcrash_async_macho_t *image, plcrash_async_symbol_cache_t *cache, pl_vm_address_t pc, plcrash_async_found_symbol_cb callback, void *ctx);

#endif
