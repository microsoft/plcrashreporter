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

#ifndef CrashReporter_PLCrashAnnotation_h
#define CrashReporter_PLCrashAnnotation_h

#include <stdint.h>

/**
 * @ingroup types
 *
 * This structure allows additional information to be associated with crashes
 * on a per-image. To make use of it, the image needs to have an annotation
 * variable placed in its __DATA,__pl_crash_info section. Any data in that
 * object at the time of the variable will be stored in the report.
 *
 * Having multiple annotation variables in a single image results in undefined
 * behavior.
 */
typedef struct plcrashreporter_image_annotation_t {
    /** The version number of this structure. The current version of this structure is 0. */
    uint16_t version;
    /** The length of the data to associate with the crash. */
    uint16_t dataLength;
#if defined(__LP64__)
    /* Explicitly insert padding to appease Clang's -Wpadded warning. */
    char _padding[4];
#endif
    /** The data to associate with the crash. */
    const void *data;
} plcrashreporter_image_annotation_t;

#ifdef __clang__
    #define PLCRASH_IMAGE_ANNOTATION_ATTRIBUTE __attribute__((section("__DATA,__pl_crash_info")))
#else
    #define PLCRASH_IMAGE_ANNOTATION_ATTRIBUTE
#endif

#endif
