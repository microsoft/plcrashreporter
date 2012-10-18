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

#include <stdint.h>
#include <mach/mach.h>

#include "PLCrashAsync.h"

typedef struct plcrash_async_macho_image {    
    /** The Mach task in which the Mach-O image can be found */
    mach_port_t task;

    /** The binary image's header address. */
    pl_vm_address_t header;
    
    /** The binary's dyld-reported reported vmaddr slide. */
    intptr_t vmaddr_slide;
    
    /** The binary image's name/path. */
    char *name;
} plcrash_async_macho_image_t;

void plcrash_async_macho_image_init (plcrash_async_macho_image_t *image, mach_port_t task, const char *name, pl_vm_address_t header, int64_t vmaddr_slide);
void plcrash_async_macho_image_free (plcrash_async_macho_image_t *image);