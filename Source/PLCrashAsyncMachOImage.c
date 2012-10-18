/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2011-2012 Plausible Labs Cooperative, Inc.
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

#include "PLCrashAsyncMachOImage.h"

#include <stdlib.h>
#include <string.h>

/**
 * @internal
 * @ingroup plcrash_async
 * @defgroup plcrash_async_image Binary Image Parsing
 *
 * Implements async-safe Mach-O binary parsing, for use at crash time when extracting binary information
 * from the crashed process.
 * @{
 */

/**
 * Initialize a new Mach-O binary image parser.
 *
 * @param image The image structure to be initialized.
 * @param name The file name or path for the Mach-O image.
 * @param header The task-local address of the image's Mach-O header.
 * @param vmaddr_slide The dyld-reported task-local vmaddr slide of this image.
 *
 * @warning This method is not async safe.
 */
void plcrash_async_macho_image_init (plcrash_async_macho_image_t *image, mach_port_t task, const char *name, uintptr_t header, intptr_t vmaddr_slide) {
    mach_port_mod_refs(mach_task_self(), task, MACH_PORT_RIGHT_SEND, 1);
    image->task = task;

    image->header = header;
    image->vmaddr_slide = vmaddr_slide;
    image->name = strdup(name);
}

/**
 * Free all Mach-O binary image resources.
 *
 * @warning This method is not async safe.
 */
void plcrash_async_macho_image_free (plcrash_async_macho_image_t *image) {
    free(image->name);
    mach_port_mod_refs(mach_task_self(), image->task, MACH_PORT_RIGHT_SEND, -1);
}


/**
 * @} plcrash_async_macho
 */