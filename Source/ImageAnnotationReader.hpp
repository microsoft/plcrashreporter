/*
 * Copyright (c) 2014 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_IMAGE_ANNOTATION_H
#define PLCRASH_ASYNC_IMAGE_ANNOTATION_H

#import "PLCrashMacros.h"
#import "PLCrashImageAnnotation.h"

#import "PLCrashAsync.h"
#import "PLCrashAsyncMachOImage.h"

/**
 * @internal
 * @ingroup plcrash_async
 * @{
 */

#ifdef __cplusplus
namespace plcrash { namespace async {

/**
 * @internal
 *
 * An async-safe PLCrashImageAnnotation reader.
 */
class ImageAnnotationReader {
public:
    ImageAnnotationReader ();
    ~ImageAnnotationReader ();
};


}} /* namespace plcrash::async */

#endif


PLCR_EXPORT plcrash_error_t plcrash_async_macho_find_annotation(plcrash_async_macho_t *image, plcrash_async_mobject_t *result);

/** @} */

#endif /* PLCRASH_ASYNC_IMAGE_ANNOTATION_H */
