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

#include "ImageAnnotationReader.hpp"

#include <inttypes.h>

/**
 * @internal
 *
 * Read the binary-specific crash annotation from the __DATA,__plcrash_info section, if available.
 *
 * @param image The image to search in
 * @param section The __DATA,__plcrash_info section of the image
 * @param result[out] On success, will be initialized with a mapped memory object containing the annotation's data.
 * It is the caller's responsibility to free the returned value via plcrash_async_mobject_free(). If the function returns an error,
 * no class_name value will be provided and the caller is not responsible for freeing any associated resources.
 *
 * @return Returns PLCRASH_ESUCCESS on success, PLCRASH_ENOTFOUND if not found, PLCRASH_EINVALID_DATA if
 * the annotation data is unparseable, or a standard plcrash_error_t error code if an error occurs reading or
 * mapping the annotation.
 */
static plcrash_error_t plcrash_async_macho_read_plcrashinfo (plcrash_async_macho_t *image, plcrash_async_mobject_t *section, plcrash_async_mobject_t *mobj)
{
    /* We're reading the PLCrashImageAnnotation structure, but can't memcpy it from the task because the layout differs between 32/64-bit
     * processes. The struct always begins with the version/length uint16s. */
    uint32_t version;
    plcrash_error_t ret = plcrash_async_mobject_read_uint32(section, image->byteorder, section->task_address, 0, &version);
    if (ret != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Failed to read version field from image annotation %d in %s", ret, image->name);
        return ret;
    }
    
    if (version != 0) {
        PLCF_DEBUG("Image annotation found with unsupported version number %" PRIu32 " in %s", version, image->name);
        return PLCRASH_EINVALID_DATA;
    }
    
    /* Read the data length */
    uint32_t data_size;
    ret = plcrash_async_mobject_read_uint32(section, image->byteorder, section->task_address, 4, &data_size);
    if (ret != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Failed to read data_size from image annotation: %d in %s", ret, image->name);
        return ret;
    }
    
    /* If the user data is zero-length, we can skip the record. */
    if (data_size == 0)
        return PLCRASH_ENOTFOUND;
    
    
    /* The size and location of the data pointer varies due to alignment. */
    pl_vm_address_t dataAddress;
    if (image->m64) {
        uint64_t ptr64;
        
        ret = plcrash_async_mobject_read_uint64(section, image->byteorder, section->task_address, 8, &ptr64);
        if (ret != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to read data pointer from the image annotation: %d in %s", ret, image->name);
            return ret;
        }
        
        dataAddress = ptr64;
    } else {
        uint32_t ptr32;
        
        ret = plcrash_async_mobject_read_uint32(section, image->byteorder, section->task_address, 8, &ptr32);
        if (ret != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to read data pointer from the image annotation: %d in %s", ret, image->name);
            return ret;
        }
        
        dataAddress = ptr32;
    }
    
    /* It's invalid to have a length specified but a NULL pointer for the data member. */
    if (dataAddress == 0) {
        PLCF_DEBUG("Image annotation is initialized with a NULL data pointer in %s", image->name);
        return PLCRASH_EINVALID_DATA;
    }
    
    /* Try to initialize a mobject containing the referenced data */
    ret = plcrash_async_mobject_init(mobj, image->task, dataAddress, data_size, true);
    if (ret != PLCRASH_ESUCCESS)
        PLCF_DEBUG("Failed to map annotation data; plcrash_async_mobject_init() returned %d in %s", ret, image->name);
    
    return ret;
}

/**
 * Fetches the binary-specific crash reporter information.
 *
 * @param image The image to search in
 * @param result[out] On success, will be initialized with a mapped memory object containing the annotation's data.
 * It is the caller's responsibility to free the returned value via plcrash_async_mobject_free(). If the function returns an error,
 * no class_name value will be provided and the caller is not responsible for freeing any associated resources.
 *
 * @return Returns PLCRASH_ESUCCESS on success, PLCRASH_ENOTFOUND if not found, PLCRASH_EINVALID_DATA if
 * the annotation data is unparseable, or a standard plcrash_error_t error code if an error occurs reading or
 * mapping the annotation.
 */
extern "C" plcrash_error_t plcrash_async_macho_find_annotation (plcrash_async_macho_t *image, plcrash_async_mobject_t *result) {
    /* Validate the arguments */
    if (image == NULL)
        return PLCRASH_EINVAL;
    
    if (result == NULL)
        return PLCRASH_EINVAL;
    
    /* Try to fetch the __plcrash_info section for the given image */
    plcrash_async_mobject_t crashinfo_section;
    plcrash_error_t ret = plcrash_async_macho_map_section(image, PLCRASH_MACHO_ANNOTATION_SEG, PLCRASH_MACHO_ANNOTATION_SECT, &crashinfo_section);
    if (ret != PLCRASH_ESUCCESS) {
        return ret;
    }
    
    /* Section found, try to read the plcrashinfo record */
    ret = plcrash_async_macho_read_plcrashinfo(image, &crashinfo_section, result);
    plcrash_async_mobject_free(&crashinfo_section);
    
    return ret;
}
