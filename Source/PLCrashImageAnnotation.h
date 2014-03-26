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

#ifndef PLCRASH_IMAGE_ANNOTATION_H
#define PLCRASH_IMAGE_ANNOTATION_H

#include <stdint.h>

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
 *
 * @todo Define atomicity requirements, or vend an API that handles atomic initialization of the annotation.
 *
 * @par Multiple Image Annotations
 * 
 * If multiple annotation variables are defined in a single image, only one of the annotations will be visible
 * to the crash reporter; which annotation is undefined. This restriction may be relaxed in a later release.
 */
typedef struct PLCrashImageAnnotation {
    /** The version number of this annotation structure. Currently the only valid value is 0. */
    uint16_t version;

    /**
     * The size in bytes of the data referenced by PLCrashImageAnnotation#data. If zero,
     * PLCrashReporter will assume no data is provided.
     */
    uint16_t data_size;
    
#if defined(__LP64__)
    /* Explicitly insert padding to appease Clang's -Wpadded warning. */
    char _padding[4];
#endif

    /**
     * A pointer to a additional per-image data that will be included in the final crash report. No restrictions
     * are placed on the format of the data; the contents are considered opaque by the crash reporter.
     */
    const void *data;
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
