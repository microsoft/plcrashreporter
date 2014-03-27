/*
 * Author: Joe Ranieri <joe@alacatialabs.com> on behalf of Xojo, Inc.
 *
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

/*
 * NOTE: This header is designed to be used independently of PLCrashReporter by third parties, and
 * will not contain any dependencies on the enclosing PLCrashReporter project.
 */

#ifndef PLCRASH_IMAGE_ANNOTATION_H
#define PLCRASH_IMAGE_ANNOTATION_H

#include <stdint.h>
#include <mach-o/loader.h> // for SEG_DATA

/** @ingroup constants
 * The Mach-O segment in which the PLCrashImageAnnotation should be placed. */
#define PLCRASH_MACHO_ANNOTATION_SEG SEG_DATA

/** @ingroup constants
 * The Mach-O section in which the PLCrashImageAnnotation should be placed. */
#define PLCRASH_MACHO_ANNOTATION_SECT "__plcrash_info"

/**
 * @ingroup types
 *
 * This structure allows additional information to be associated with crashes on a per-image basis.
 *
 * At crash time, the crash reporter will walk all loaded Mach-O images, and including any associated
 * per-image annotations in the final crash report.
 *
 * @par Using Image Annotations
 *
 * To declare an annotation that will be visible to the crash reporter, the annotation data must:
 *
 * - Be placed in the appropriate binary section and segment (@sa PLCRASH_IMAGE_ANNOTATION_ATTRIBUTE).
 * - Be initialized with a valid version number and data pointer, and data_size.
 * - Use atomic primitives to ensure that a crash reporter will read a fully consistent record.
 *
 * In addition, if it's possible that an annotation record may be written to by multiple threads, the
 * writer must ensure synchronization of updates to the annotation data.
 *
 * @par Multiple Image Annotations
 * 
 * If multiple annotation variables are defined in a single image, only one of the annotations will be visible
 * to the crash reporter; which annotation is undefined. This restriction may be relaxed in a later release.
 *
 * @par Example Usage
 @code
 #include "PLCrashImageAnnotation.h"
 #include <pthread.h>
 #include <libkern/OSAtomic.h>
 
 pthread_mutex_t image_annotation_lock = PTHREAD_MUTEX_INITIALIZER;
 PLCrashImageAnnotation image_annotation PLCRASH_IMAGE_ANNOTATION_ATTRIBUTE = {
 .version = 0,
 .data = NULL,
 .data_size = 0
 };
 
 void set_image_annotation (void *data, size_t data_size) {
 pthread_mutex_lock(&image_annotation_lock);
 
 // Ensure that the crash reporter skips the annotation while we're initializing it
 OSAtomicCompareAndSwap32Barrier(image_annotation.data_size, 0, (volatile int32_t *) &image_annotation.data_size);
 
 // Initialize the data pointer
 image_annotation.data = data;
 
 // Atomically initialize the data_size value, making our now fully initialized annotation
 // visible to the crash reporter.
 OSAtomicCompareAndSwap32Barrier(image_annotation.data_size, data_size, (volatile int32_t *) &image_annotation.data_size);
 
 pthread_mutex_unlock(&image_annotation_lock);
 }
 @endcode
 */
typedef struct PLCrashImageAnnotation {
    /** The version number of this annotation structure. Currently the only valid value is 0. */
    volatile uint32_t version;

    /**
     * The size in bytes of the data referenced by PLCrashImageAnnotation#data. If zero,
     * PLCrashReporter will assume no data is provided.
     */
    volatile uint32_t data_size;

    /**
     * A pointer to a additional per-image data that will be included in the final crash report. No restrictions
     * are placed on the format of the data; the contents are considered opaque by the crash reporter.
     */
    volatile const void *data;
} PLCrashImageAnnotation;

/**
 * @ingroup constants
 *
 * A compiler attribute that will place a global PLCrashImageAnnotation variable in the appropriate Mach-O segment and
 * section (__DATA, __plcrash).
 *
 * Example Usage:
 *
 @code
 PLCrashImageAnnotation crashImageAnnotation PLCRASH_IMAGE_ANNOTATION_ATTRIBUTE;
 @endcode
 */
#define PLCRASH_IMAGE_ANNOTATION_ATTRIBUTE __attribute__((section(PLCRASH_MACHO_ANNOTATION_SEG "," PLCRASH_MACHO_ANNOTATION_SECT)))

#endif /* PLCRASH_IMAGE_ANNOTATION_H */
