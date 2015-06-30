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

#ifndef PLCRASH_DYNAMIC_LOADER_H
#define PLCRASH_DYNAMIC_LOADER_H

#include <mach/mach.h>
#include <mach/task_info.h>

#include "PLCrashMacros.h"
#include "PLCrashAsync.h"

#include "AsyncAllocator.hpp"
#include "AsyncAllocatable.hpp"

#include "PLCrashAsyncMachOImage.h"
#include "PLCrashAsyncAllocator.h"

PLCR_CPP_BEGIN_ASYNC_NS

/* Forward declaration */
template<typename machine_ptr> class DyldImageInfo;

/**
 * An immutable reference to a source of dynamic loader data for
 * a specific target task.
 */
class DynamicLoader : public AsyncAllocatable {
public:
    /**
     * An immutable array of Mach-O images.
     */
    class ImageList : public AsyncAllocatable {
        template<typename> friend class DyldImageInfo;
        
    public:
        static plcrash_error_t NonAsync_Read (ImageList **imageList, AsyncAllocator *allocator, task_t task);

        /* Copy/move are not supported. */
        ImageList (const ImageList &) = delete;
        ImageList (ImageList &&) = delete;
        
        ImageList &operator= (const ImageList &) = delete;
        ImageList &operator= (ImageList &&) = delete;
        

        plcrash_async_macho_t *getImage (size_t index);
        size_t count ();
        
        plcrash_async_macho_t *imageContainingAddress (pl_vm_address_t address);
        
        ImageList ();
        ~ImageList ();

    private:
        /**
         * Construct a new image list; the new list will assume ownership of @a images.
         *
         * @param allocator A borrowed reference to the allocator to be used to deallocate @a images.
         * @param images An array of images. Ownership of this value will be assumed.
         * @param count The total number of images in @a images.
         */
        ImageList (AsyncAllocator *allocator, plcrash_async_macho_t *images, size_t count) : _allocator(allocator), _images(images), _count(count) {}
        
        /** A borrowed reference to the allocator to be used to deallocate _images */
        AsyncAllocator *_allocator;
        
        /** The array of Mach-O image instances. */
        plcrash_async_macho_t *_images;

        /** The number of images in this image list. */
        size_t _count;
    };
    
    static plcrash_error_t NonAsync_Create (DynamicLoader **loader, AsyncAllocator *allocator, task_t task);
    
    plcrash_error_t readImageList (AsyncAllocator *allocator, DynamicLoader::ImageList **image_list);
    
    ~DynamicLoader ();
    
    /** Copy constructor */
    DynamicLoader (const DynamicLoader &other) : DynamicLoader(other._task, other._dyld_info) {}

    /** Move constructor */
    DynamicLoader (DynamicLoader &&other) : _task(other._task), _dyld_info(other._dyld_info) {
        other._task = MACH_PORT_NULL;
    }
    
    /** Copy assignment operator */
    DynamicLoader &operator= (const DynamicLoader &other) {
        /* Update our task reference */
        setTask(other._task);
        
        /* Update dyld info */
        _dyld_info = other._dyld_info;
        
        return *this;
    }
    
    /** Move assignment operator */
    DynamicLoader &operator= (DynamicLoader &&other) {
        /* Borrow the other instance's task reference */
        _task = other._task;
        other._task = MACH_PORT_NULL;
        
        /* Update dyld info */
        _dyld_info = other._dyld_info;
        
        return *this;
    }
    
    /**
     * Return the task dyld info.
     */
    struct task_dyld_info dyld_info () { return _dyld_info; }
    
private:
    DynamicLoader (task_t task, struct task_dyld_info dyld_info);
    void setTask (task_t new_task);
    
    /** The task containing the fetched dyld info. */
    task_t _task;

    /** The dyld info fetched from _task */
    struct task_dyld_info _dyld_info;
};

PLCR_CPP_END_ASYNC_NS

#endif /* PLCRASH_DYNAMIC_LOADER_H */
