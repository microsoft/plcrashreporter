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

#include "DynamicLoader.hpp"
#include <inttypes.h>

#include "PLCrashAsyncMObject.h"
#include "PLCrashAsyncMachOString.h"

PLCR_CPP_BEGIN_ASYNC_NS

/**
 * @internal
 *
 * 32/64-bit-independent definitions of dyld_image.h structures. We can reasonably assume a shared byte order given that
 * this API can never be used against on-disk data.
 *
 * @tparam machine_ptr Pointer-width type for the target architecture.
 */
template <typename machine_ptr> class DyldImageInfo {
private:
    /**
     * Pointer-size independent ABI-compatible definition of the dyld_image_info structure.
     */
    struct image_info {
        /** The image's base address. */
        machine_ptr     _imageLoadAddress;
        
        /** The dyld image path. */
        machine_ptr     _imageFilePath;
        
        /** The (time_t) modification time of the file, recorded by dyld when first loading the image. */
        uintptr_t       _imageFileModDate;
    };
    
    /**
     * Pointer-size independent ABI-compatible definition of the dyld_all_image_infos structure.
     */
    struct all_image_infos {
        /** Image info structure version. This should be version >= 1. */
        uint32_t        _version;
        
        /** The number of entries in the info array */
        uint32_t        _infoArrayCount;
        
        /** A pointer to an array of `struct dyld_image_info` values */
        machine_ptr     _infoArray;
    };
    
public:
    /**
     * Read the image list from @a target, returning a valid image list via @a image_list on success. The caller
     * is responsible for deallocating a non-NULL result value via `delete`.
     *
     * @param allocator The allocator to be used when instantiating the image list.
     * @param task The task from which to read the image list.
     * @param dyld_info The dyld info for @a target.
     * @param image_list On success, a newly allocated DynamicLoader::ImageList.
     *
     * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned.
     */
    static plcrash_error_t readImageList (AsyncAllocator *allocator, task_t task, struct task_dyld_info &dyld_info, DynamicLoader::ImageList **image_list) {
        all_image_infos all_infos;
        size_t invalid_info_count;
        size_t image_list_count;
        plcrash_error_t err;
        image_info *info;

        /* Fetch the subset of the dyld_all_image_infos structure we're interested in */
        if ((err = plcrash_async_task_memcpy(task, dyld_info.all_image_info_addr, 0, &all_infos, sizeof(all_infos))) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to read dyld_all_image_infos structure at address 0x%" PRIx64 ": %d", (uint64_t) dyld_info.all_image_info_addr, err);
            return err;
        }
        
        /* Validate the version number. */
        if (all_infos._version < 1) {
            PLCF_DEBUG("Invalid version %" PRIu32 "for dyld_all_image_infos structure at address 0x%" PRIx64, all_infos._version, (uint64_t) dyld_info.all_image_info_addr);
            return PLCRASH_EINVALID_DATA;
        }
        
        
        /* Sanity-check the infoArrayCount for overflow of vm_size_t */
        if (PL_VM_SIZE_MAX / sizeof(image_info) < all_infos._infoArrayCount) {
            PLCF_DEBUG("infoArrayCount of %" PRIu32 " would overflow pl_vm_size_t!", all_infos._infoArrayCount);
            return PLCRASH_EINVALID_DATA;
        }

        /* Fetch a reference to the info array */
        plcrash_async_mobject_t mobj;
        err = plcrash_async_mobject_init(&mobj, task, all_infos._infoArray, all_infos._infoArrayCount * sizeof(image_info), true);
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to map dyld_image_info array at address 0x%" PRIx64 ": %d", (uint64_t) dyld_info.all_image_info_addr, err);
            return err;
        }
        
        /* Allocate our destination array of plcrash_async_macho_t instances */
        plcrash_async_macho_t *images_array;
        if ((err != allocator->alloc((void **) &images_array, sizeof(*images_array) * all_infos._infoArrayCount)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to allocate plcrash_async_macho_t array: %d", err);
            goto cleanup;
        }

        /* Fetch the base address of the array. This should always succeed given that we validated this exact same range
         * by performing the mapping in the first place. */
        info = (image_info *) plcrash_async_mobject_remap_address(&mobj, all_infos._infoArray, 0, sizeof(image_info) * all_infos._infoArrayCount);
        if (info == NULL) {
            PLCF_DEBUG("Unexpected mobject mapping error when iterating over dyld_image_info!");
            err = PLCRASH_EUNKNOWN;
            goto cleanup;
            
        }
        
        /* Initialize our plcrash_async_macho_t from the all_infos array. */
        invalid_info_count = 0;
        for (uint32_t i = 0; i < all_infos._infoArrayCount; i++) {
            plcrash_async_macho_string_t name_str;
            const char *name = NULL;

            /* Try to fetch a reference to the image's path string. Failure to fetch the name is not a fatal error, but it should generally not happen. */
            if ((err = plcrash_async_macho_string_init(&name_str, task, info->_imageFilePath)) != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("Failed to initialize plcrash_async_macho_string_t for string address %" PRIu64 ": %d", (uint64_t) info->_imageFilePath, err);
            } else {
                if ((err = plcrash_async_macho_string_get_pointer(&name_str, &name)) != PLCRASH_ESUCCESS) {
                    PLCF_DEBUG("Failed to fetch image name string pointer at address %" PRIu64 ": %d", (uint64_t) info->_imageFilePath, err);
                }
            }
            
            /* Initialize our actual mach-o image instance */
            if ((err = plcrash_async_macho_init(&images_array[i - invalid_info_count], allocator, task, name, info->_imageLoadAddress)) != PLCRASH_ESUCCESS) {
                /* If we can't read the image, update the failed image count and continue. We still want to gather as many images
                 * as we can. */
                PLCF_DEBUG("Failed to load Mach-O image info from base address %" PRIu64 ", skipping: %d", (uint64_t) info->_imageLoadAddress, err);
                
                invalid_info_count++;
            }
            
            /* Iterate */
            info++;
            
            /* Clean up! */
            plcrash_async_macho_string_free(&name_str);
        }
        
        /* Success! We just need to adjust our total count and return our ImageList. */
        image_list_count = all_infos._infoArrayCount - invalid_info_count;
        *image_list = new (allocator) DynamicLoader::ImageList(allocator, images_array, image_list_count);
        err = PLCRASH_ESUCCESS;
        
        // Fall-through
        
        /* Clean up */
    cleanup:
        plcrash_async_mobject_free(&mobj);
        return err;
    };
};

/**
 * Attempt to construct a DynamicLoader reference for @a task, placing a pointer to the allocated
 * instance in @a loader on success.
 *
 * @param allocator The allocator to be used when instantiating the new DynamicLoader instance.
 * @param[out] loader On success, will be initialized with a pointer to a new DynamicLoader instance. It is the caller's
 * responsibility to free this instance via `delete`.
 * @param task The target task.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned.
 *
 * @warning This method is not gauranteed async-safe.
 */
plcrash_error_t DynamicLoader::NonAsync_Create (DynamicLoader **loader, AsyncAllocator *allocator, task_t task) {
    kern_return_t kr;

    /* Try to fetch the task info. */
    task_dyld_info_data_t data;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    
    kr = task_info(task, TASK_DYLD_INFO, (task_info_t) &data, &count);
    if (kr != KERN_SUCCESS) {
        PLCF_DEBUG("Fetch of TASK_DYLD_INFO failed with Mach error: %d", kr);
        return PLCRASH_EINTERNAL;
    }
    
    /* Success! Initialize via our copy/move constructor */
    *loader = new (allocator) DynamicLoader(task, data);
    return PLCRASH_ESUCCESS;
}

/**
 * Attempt to read the image list from the dynamic loader, returning a valid image list via @a image_list on success. The caller
 * is responsible for deallocating a non-NULL @a image_list value via `delete`.
 *
 * @param allocator The allocator to be used when instantiating the image list.
 * @param image_list On success, a newly allocated DynamicLoader::ImageList.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned.
 */
plcrash_error_t DynamicLoader::readImageList (AsyncAllocator *allocator, DynamicLoader::ImageList **image_list) {
    switch (_dyld_info.all_image_info_format) {
        case TASK_DYLD_ALL_IMAGE_INFO_32:
            return DyldImageInfo<uint32_t>::readImageList(allocator, _task, _dyld_info, image_list);
            
        case TASK_DYLD_ALL_IMAGE_INFO_64:
            return DyldImageInfo<uint64_t>::readImageList(allocator, _task, _dyld_info, image_list);
            
        default:
            PLCF_DEBUG("The fetched TASK_DYLD_INFO contains an unknown info format value: %d", _dyld_info.all_image_info_format);
            return PLCRASH_EINVALID_DATA;
    }
}

/**
 * Construct a new DynamicLoader instance with the provided target @a task and the task's @a dyld_info.
 *
 * @param task The target task.
 * @param dyld_info The TASK_DYLD_INFO data fetched from @a task.
 */
DynamicLoader::DynamicLoader (task_t task, struct task_dyld_info dyld_info) : _task(MACH_PORT_NULL), _dyld_info(dyld_info) {
    /* Add a proper refcount for the task */
    setTask(task);
}

/**
 * Update the backing task port, handling reference counting of our send right.
 *
 * @param new_task The new task port value. May be MACH_PORT_NULL or MACH_PORT_DEAD.
 */
void DynamicLoader::setTask (task_t new_task) {
    /* Is there anything to do? */
    if (_task == new_task)
        return;
    
    /* Drop our send right reference. */
    if (MACH_PORT_VALID(_task))
        mach_port_mod_refs(mach_task_self(), _task, MACH_PORT_RIGHT_SEND, -1);
    
    /* Save the new task port value */
    _task = new_task;
    
    /* Acquire a send right reference, keeping the new task port alive. */
    if (MACH_PORT_VALID(_task))
        mach_port_mod_refs(mach_task_self(), _task, MACH_PORT_RIGHT_SEND, 1);
}

DynamicLoader::~DynamicLoader () {
    /* Discard our task port reference, if any */
    setTask(MACH_PORT_NULL);
}

/**
 * Attempt to read an image list directly from @a task, placing a pointer to the allocated
 * list instance in @a imageList on success.
 *
 * This method is equivalent to combining the non-async-safe DynamicLoader::NonAsync_Create() with the async-safe
 * DynamicLoader::readImageList(), and is primarily useful when operating outside of a mandatory async-safe
 * context.
 *
 * @param allocator The allocator to be used when instantiating the new ImageList instance.
 * @param[out] imageList On success, will be initialized with a pointer to a new ImageList instance. It is the caller's
 * responsibility to free this instance via `delete`.
 * @param task The target task.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned.
 *
 * @warning This method is not gauranteed async-safe.
 */
plcrash_error_t DynamicLoader::ImageList::NonAsync_Read (DynamicLoader::ImageList **imageList, AsyncAllocator *allocator, task_t task) {
    DynamicLoader *loader;
    plcrash_error_t err;
    
    /* Instantiate a loader instance for reading */
    if ((err = DynamicLoader::NonAsync_Create(&loader, allocator, task)) != PLCRASH_ESUCCESS)
        return err;
    
    /* Perform the read */
    err = loader->readImageList(allocator, imageList);
    
    /* Clean up the no-longer-necessary loader instance and return. */
    delete loader;
    return err;
}

/**
 * Construct an empty image list.
 */
DynamicLoader::ImageList::ImageList () : _allocator(NULL), _images(NULL), _count(0) {}

/**
 * Return a borrowed reference to the Mach-O image entry at @a index.
 */
plcrash_async_macho_t *DynamicLoader::ImageList::getImage (size_t index) {
    PLCF_ASSERT(index < _count);
    return &_images[index];
}

/**
 * Return the total number of available images.
 */
size_t DynamicLoader::ImageList::count () { return _count; }

/**
 * Return a borrowed reference to image containing the given @a address within its TEXT segment.
 * If image is not found, NULL will be returned.
 *
 * @param address The target-relative address to be searched for.
 *
 * @warning The list must be retained for reading via plcrash_async_image_list_set_reading() before calling this function.
 */
plcrash_async_macho_t *DynamicLoader::ImageList::imageContainingAddress (pl_vm_address_t address) {
    for (size_t i = 0; i < _count; i++) {
        if (plcrash_async_macho_contains_address(getImage(i), address))
            return getImage(i);
    }
    
    /* Not found */
    return NULL;
}

DynamicLoader::ImageList::~ImageList () {
    if (_images != NULL) {
        /* Clean up all image instances. */
        for (size_t i = 0; i < _count; i++) {
            plcrash_async_macho_free(&_images[i]);
        }
        
        /* And now the actual array */
        _allocator->dealloc(_images);
    }
}

PLCR_CPP_END_ASYNC_NS
