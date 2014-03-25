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

/** The Mach-O segment in which the PLCrashImageAnnotation should be placed. */
#define PLCRASH_MACHO_ANNOTATION_SEG SEG_DATA

/** The Mach-O section in which the PLCrashImageAnnotation should be placed. */
#define PLCRASH_MACHO_ANNOTATION_SECT "__plcrash_info"

/**
 * @ingroup types
 *
 * This structure allows additional information to be associated with crashes
 * on a per-image. To make use of it, the image needs to have an annotation
 * variable placed in its __DATA,__plcrash_info section. Any data in that
 * object at the time of the variable will be stored in the report.
 *
 * Having multiple annotation variables in a single image results in undefined behavior.
 */
typedef struct PLCrashImageAnnotation {
    /** The version number of this structure. The current version of this structure is 0. */
    uint16_t version;
    /** The length of the data to associate with the crash. */
    uint16_t data_size;
#if defined(__LP64__)
    /* Explicitly insert padding to appease Clang's -Wpadded warning. */
    char _padding[4];
#endif
    /** The data to associate with the crash. */
    const void *data;
} PLCrashImageAnnotation;

#ifdef __clang__
    #define PLCRASH_IMAGE_ANNOTATION_ATTRIBUTE __attribute__((section(PLCRASH_MACHO_ANNOTATION_SEG "," PLCRASH_MACHO_ANNOTATION_SECT)))
#else
    #define PLCRASH_IMAGE_ANNOTATION_ATTRIBUTE
#endif

#endif /* PLCRASH_IMAGE_ANNOTATION_H */
