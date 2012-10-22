/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2009 Plausible Labs Cooperative, Inc.
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

#import "PLCrashAsync.h"

#import <stdint.h>
#import <errno.h>
#import <string.h>

/**
 * @internal
 * @defgroup plcrash_async Async Safe Utilities
 * @ingroup plcrash_internal
 *
 * Implements async-safe utility functions
 *
 * @{
 */

/**
 * Return an error description for the given plcrash_error_t.
 */
const char *plcrash_strerror (plcrash_error_t error) {
    switch (error) {
        case PLCRASH_ESUCCESS:
            return "No error";
        case PLCRASH_EUNKNOWN:
            return "Unknown error";
        case PLCRASH_OUTPUT_ERR:
            return "Output file can not be opened (or written to)";
        case PLCRASH_ENOMEM:
            return "No memory available";
        case PLCRASH_ENOTSUP:
            return "Operation not supported";
        case PLCRASH_EINVAL:
            return "Invalid argument";
        case PLCRASH_EINTERNAL:
            return "Internal error";
        case PLCRASH_EACCESS:
            return "Access denied";
        case PLCRASH_ENOTFOUND:
            return "Not found";
    }
    
    /* Should be unreachable */
    return "Unhandled error code";
}

/**
 * Initialize a new memory object reference, mapping @a task_addr from @a task into the current process. The mapping
 * will be copy-on-write, and will be checked to ensure a minimum protection value of VM_PROT_READ.
 *
 * @param mobj Memory object to be initialized.
 * @param task The task from which the memory will be mapped.
 * @param task_address The task-relative address of the memory to be mapped. This is not required to fall on a page boundry.
 * @param length The total size of the mapping to create.
 *
 * @return On success, returns PLCRASH_ESUCCESS. On failure, one of the plcrash_error_t error values will be returned, and no
 * mapping will be performed.
 *
 * @warn Callers must call plcrash_async_mobject_free() on @a mobj, even if plcrash_async_mobject_init() fails.
 */
plcrash_error_t plcrash_async_mobject_init (plcrash_async_mobject_t *mobj, mach_port_t task, pl_vm_address_t task_addr, pl_vm_size_t length) {
    /* We must first initialize vm_address to 0x0. We'll check this in _free() to determine whether calling vm_deallocate() is required */
    mobj->vm_address = 0x0;

    kern_return_t kt;
    
    /* Compute the total required page size. */
    pl_vm_size_t page_size = mach_vm_round_page(length + (task_addr - mach_vm_trunc_page(task_addr)));

    /* Remap the target pages into our process */
    vm_prot_t cur_prot;
    vm_prot_t max_prot;

#ifdef PL_HAVE_MACH_VM
    kt = mach_vm_remap(mach_task_self(), &mobj->vm_address, page_size, 0x0, TRUE, task, task_addr, FALSE, &cur_prot, &max_prot, VM_INHERIT_COPY);
#else
    kt = vm_remap(mach_task_self(), &mobj->vm_address, page_size, 0x0, TRUE, task, task_addr, FALSE, &cur_prot, &max_prot, VM_INHERIT_COPY);
#endif

    if (kt != KERN_SUCCESS) {
        PLCF_DEBUG("vm_remap() failure: %d at %s:%d\n", kt, __FILE__, __LINE__);
        // Should we use more descriptive errors?
        return PLCRASH_ENOMEM;
    }

    if ((cur_prot & VM_PROT_READ) == 0) {
        return PLCRASH_EACCESS;
    }

    /* Determine the offset to the actual data */
    mobj->address = mobj->vm_address + (task_addr - mach_vm_trunc_page(task_addr));
    mobj->length = length;

    /* Determine the difference between the target and local mappings. Note that this needs to be computed on either two page
     * aligned addresses, or two non-page aligned addresses. Mixing task_addr and vm_address would return an incorrect offset. */
    mobj->vm_slide = task_addr - mobj->address;

    return PLCRASH_ESUCCESS;
}

/**
 * Apply @a mobj's computed slide to @a address, mapping a pointer from the target task into the local address space.
 *
 * The validity of the pointer is not verified. The plcrash_async_mobject_pointer() function must be used to validate
 * the returned address and convert it to a pointer value.
 *
 * @param mobj An initialized memory object.
 * @param address An address within the memory mapped from the target task's address space via @a mobj.
 */
uintptr_t plcrash_async_mobject_remap_address (plcrash_async_mobject_t *mobj, pl_vm_address_t address) {
    return address - mobj->vm_slide;
}

/**
 * Validate an address pointer's availability via @a mobj, verifying that @a length bytes can be read from @a mobj at
 * @a address, and return the pointer from which a @a length read may be performed.
 *
 * @param mobj An initialized memory object.
 * @param address The address to be read. This address should be relative to the current task, rather than relative
 * to the task from which @a mobj was mapped.
 * @param length The total number of bytes that should be readable at @a address.
 *
 * @return Returns the validated pointer, or NULL if the requested bytes are not within @a mobj's range.
 */
void *plcrash_async_mobject_pointer (plcrash_async_mobject_t *mobj, uintptr_t address, size_t length) {
    /* Verify that the address starts within range */
    if (address < mobj->address)
        return NULL;

    /* Verify that the address value won't overrun */
    if (UINTPTR_MAX - length < address)
        return NULL;

    /* Check that the block ends within range */
    if (mobj->address + mobj->length < address + length)
        return NULL;

    return (void *) address;
}

/**
 * Free the memory mapping.
 */
void plcrash_async_mobject_free (plcrash_async_mobject_t *mobj) {
    if (mobj->vm_address == 0x0)
        return;

    kern_return_t kt;
    if ((kt = vm_deallocate(mach_task_self(), mobj->address, mobj->length)) != KERN_SUCCESS)
        PLCF_DEBUG("vm_deallocate() failure: %d at %s:%d\n", kt, __FILE__, __LINE__);
}

/**
 * (Safely) read len bytes from @a source, storing in @a dest.
 *
 * @param task The task from which data from address @a source will be read.
 * @param source The address within @a task from which the data will be read.
 * @param dest The destination address to which copied data will be written.
 * @param len The number of bytes to be read.
 *
 * @return On success, returns KERN_SUCCESS. If the pages containing @a source + len are unmapped, KERN_INVALID_ADDRESS
 * will be returned. If the pages can not be read due to access restrictions, KERN_PROTECTION_FAILURE will be returned.
 *
 * @warning Unlike all other plcrash_* functions, plcrash_async_read_addr returns a kern_return_t value.
 * @todo Modify plcrash_async_read_addr and all API clients to use plcrash_error_t values.
 */
kern_return_t plcrash_async_read_addr (mach_port_t task, pl_vm_address_t source, void *dest, pl_vm_size_t len) {
#ifdef PL_HAVE_MACH_VM
    pl_vm_size_t read_size = len;
    return mach_vm_read_overwrite(task, source, len, (pointer_t) dest, &read_size);
#else
    vm_size_t read_size = len;
    return vm_read_overwrite(task, source, len, (pointer_t) dest, &read_size);
#endif
}

/**
 * An intentionally naive async-safe implementation of strcmp(). strcmp() itself is not declared to be async-safe,
 * though in reality, it is.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @return Return an integer greater than, equal to, or less than 0, according as the string @a s1 is greater than,
 * equal to, or less than the string @a s2.
 */
int plcrash_async_strcmp(const char *s1, const char *s2) {
    while (*s1 == *s2++) {
        if (*s1++ == 0)
            return (0);
    }

    return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

/**
 * An intentionally naive async-safe implementation of memcpy(). memcpy() itself is not declared to be async-safe.
 *
 * @param dest Destination.
 * @param source Source.
 * @param n Number of bytes to copy.
 */
void *plcrash_async_memcpy (void *dest, const void *source, size_t n) {
    uint8_t *s = (uint8_t *) source;
    uint8_t *d = (uint8_t *) dest;

    for (size_t count = 0; count < n; count++)
        *d++ = *s++;

    return (void *) source;
}

/**
 * @internal
 * @ingroup plcrash_async
 * @defgroup plcrash_async_bufio Async-safe Buffered IO
 * @{
 */

/**
 * 
 * Write len bytes to fd, looping until all bytes are written
 * or an error occurs. For the local file system, only one call to write()
 * should be necessary
 */
static ssize_t writen (int fd, const void *data, size_t len) {
    const void *p;
    size_t left;
    ssize_t written = 0;
    
    /* Loop until all bytes are written */
    p = data;
    left = len;
    while (left > 0) {
        if ((written = write(fd, p, left)) <= 0) {
            if (errno == EINTR) {
                // Try again
                written = 0;
            } else {
                PLCF_DEBUG("Error occured writing to crash log: %s", strerror(errno));
                return -1;
            }
        }
        
        left -= written;
        p += written;
    }
    
    return written;
}


/**
 * Initialize the plcrash_async_file_t instance.
 *
 * @param file File structure to initialize.
 * @param output_limit Maximum number of bytes that will be written to disk. Intended as a
 * safety measure prevent a run-away crash log writer from filling the disk. Specify
 * 0 to disable any limits. Once the limit is reached, all data will be dropped.
 * @param fd Open file descriptor.
 */
void plcrash_async_file_init (plcrash_async_file_t *file, int fd, off_t output_limit) {
    file->fd = fd;
    file->buflen = 0;
    file->total_bytes = 0;
    file->limit_bytes = output_limit;
}


/**
 * Write all bytes from @a data to the file buffer. Returns true on success,
 * or false if an error occurs.
 */
bool plcrash_async_file_write (plcrash_async_file_t *file, const void *data, size_t len) {
    /* Check and update output limit */
    if (file->limit_bytes != 0 && len + file->total_bytes > file->limit_bytes) {
        return false;
    } else if (file->limit_bytes != 0) {
        file->total_bytes += len;
    }

    /* Check if the buffer will fill */
    if (file->buflen + len > sizeof(file->buffer)) {
        /* Flush the buffer */
        if (writen(file->fd, file->buffer, file->buflen) < 0) {
            return false;
        }
        
        file->buflen = 0;
    }
    
    /* Check if the new data fits within the buffer, if so, buffer it */
    if (len + file->buflen <= sizeof(file->buffer)) {
        plcrash_async_memcpy(file->buffer + file->buflen, data, len);
        file->buflen += len;
        
        return true;
        
    } else {
        /* Won't fit in the buffer, just write it */
        if (writen(file->fd, data, len) < 0) {
            return false;
        }
        
        return true;
    } 
}


/**
 * Flush all buffered bytes from the file buffer.
 */
bool plcrash_async_file_flush (plcrash_async_file_t *file) {
    /* Anything to do? */
    if (file->buflen == 0)
        return true;
    
    /* Write remaining */
    if (writen(file->fd, file->buffer, file->buflen) < 0)
        return false;
    
    file->buflen = 0;
    
    return true;
}


/**
 * Close the backing file descriptor.
 */
bool plcrash_async_file_close (plcrash_async_file_t *file) {
    /* Flush any pending data */
    if (!plcrash_async_file_flush(file))
        return false;

    /* Close the file descriptor */
    if (close(file->fd) != 0) {
        PLCF_DEBUG("Error closing file: %s", strerror(errno));
        return false;
    }

    return true;
}

/**
 * @} plcrash_async_bufio
 */

/**
 * @} plcrash_async
 */