/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2013 - 2015 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_DYNAMICLOADER_C_COMPAT_H
#define PLCRASH_ASYNC_DYNAMICLOADER_C_COMPAT_H

#include "PLCrashMacros.h"
#include "PLCrashAsyncAllocator.h"
#include "PLCrashAsyncMachOImage.h"

/*
 * Provides a pure C interface to the C++ DynamicLoader.
 *
 * This exists solely to support the remaining C/Objective-C code; once the codebase migrates to C/C++/Objective-C++,
 * we can drop the C compatibility support used here.
 */

#ifdef __cplusplus
#include "DynamicLoader.hpp"
using plcrash_async_dynloader_t = plcrash::async::DynamicLoader;
using plcrash_async_image_list_t = plcrash::async::DynamicLoader::ImageList;
#else
typedef struct plcrash_async_dynloader plcrash_async_dynloader_t;
typedef struct plcrash_async_image_list plcrash_async_image_list_t;
#endif


PLCR_C_BEGIN_DECLS

plcrash_error_t plcrash_nasync_dynloader_new (plcrash_async_dynloader_t **loader, plcrash_async_allocator_t *allocator, task_t task);
plcrash_error_t plcrash_async_dynloader_read_image_list (plcrash_async_dynloader_t *loader, plcrash_async_allocator_t *allocator, plcrash_async_image_list_t **image_list);
void plcrash_async_dynloader_free (plcrash_async_dynloader_t *loader);


plcrash_error_t plcrash_nasync_image_list_new (plcrash_async_image_list_t **list, plcrash_async_allocator_t *allocator, task_t task);

plcrash_async_image_list_t *plcrash_async_image_list_new_empty (plcrash_async_allocator_t *allocator);

plcrash_async_macho_t *plcrash_async_image_list_get_image (plcrash_async_image_list_t *list, size_t index);
size_t plcrash_async_image_list_count (plcrash_async_image_list_t *list);
plcrash_async_macho_t *plcrash_async_image_containing_address (plcrash_async_image_list_t *list, pl_vm_address_t address);
void plcrash_async_image_list_free (plcrash_async_image_list_t *list);

PLCR_C_END_DECLS

#endif /* PLCRASH_ASYNC_DYNAMICLOADER_C_COMPAT_H */
