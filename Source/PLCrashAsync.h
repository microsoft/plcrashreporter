/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_H
#define PLCRASH_ASYNC_H

#include <stdio.h> // for snprintf
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>

#include <TargetConditionals.h>
#include <mach/mach.h>

#if TARGET_OS_IPHONE

/**
 * @internal
 *
 * PLCrashReporter uses vm_remap() to atomically validate that a target memory range is valid, to ensure that it will
 * remain valid throughought the report generation process, and to transparently support out-of-process execution by
 * remapping pages from the target task. This replaced previous use of vm_read_overwrite(), which required a syscall
 * and a copy for every memory read; the cost of which was unsuitable for any sufficiently large read operations, such
 * as reading __LINKEDIT and the Objective-C metadata.
 *
 * If this macro is defined, the target architecture has a broken vm_remap() or mach_vm_remap() implementation that
 * results in a kernel panic. This appears to be the case on iOS 6.0 through 6.1.2, possibly fixed in 6.1.3. Note that
 * no stable release of PLCrashReporter shipped with the vm_remap() code.
 *
 * Investigation of the failure seems to show an over-release of the target vm_map and backing vm_object, leading to
 * NULL dereference, invalid memory references, and in some cases, deadlocks that result in watchdog timeouts.
 *
 * In one example case, the crash occurs in update_first_free_ll() as a NULL dereference of the vm_map_entry_t parameter.
 * Analysis of the limited reports shows that this is called via vm_map_store_update_first_free(). No backtrace is
 * available from the kernel panics, but analyzing the register state demonstrates:
 * - A reference to vm_map_store_update_first_free() remains in the link register.
 * - Of the following callers, one can be eliminated by register state:
 *     - vm_map_enter - not possible, r3 should be equal to r0
 *     - vm_map_clip_start - possible
 *     - vm_map_clip_unnest - possible
 *     - vm_map_clip_end - possible
 *
 * In the other panic seen in vm_object_reap_pages(), a value of 0x8008 is loaded and deferenced from the next pointer
 * of an element within the vm_object's resident page queue (object->memq).
 *
 * Unfortunately, our ability to investigate has been extremely constrained by the following issues;
 * - The panic is not easily or reliably reproducible
 * - Apple's does not support iOS kernel debugging
 * - There is no support for jailbreak kernel debugging against iOS 6.x devices at the time of writing.
 *
 * The work-around deployed here is to split the vm_remap() into distinct calls to mach_make_memory_entry_64() and
 * vm_map(); this follows a largely distinct code path from vm_remap(). In testing by a large-scale user of PLCrashReporter,
 * they were no longer able to reproduce the issue with this fix in place. Additionally, they've not been able to reproduce
 * the issue on 6.1.3 devices, or had any reports of the issue occuring on 6.1.3 devices.
 */
#define PL_HAVE_BROKEN_VM_REMAP 1

#endif /* TARGET_OS_IPHONE */


#if TARGET_OS_IPHONE

/*
 * iOS does not provide the mach_vm_* APIs, and as such, we can't support both
 * 32-bit/64-bit tasks via the same APIs.
 *
 * In practice, this currently does not matter for iOS, as no 64-bit ARM CPU
 * exists.
 */

/** The largest address value that can be represented via the pl_vm_address_t type. */
#ifdef __LP64__
#define PL_VM_ADDRESS_MAX UINT64_MAX
#else
#define PL_VM_ADDRESS_MAX UINT32_MAX
#endif

/** The largest address value that can be represented via the pl_vm_size_t type. */
#ifdef __LP64__
#define PL_VM_SIZE_MAX UINT64_MAX
#else
#define PL_VM_SIZE_MAX UINT32_MAX
#endif

/** VM address type. 
 * @ingroup plcrash_async */
typedef vm_address_t pl_vm_address_t;

/** VM size type.
 * @ingroup plcrash_async */
typedef vm_size_t pl_vm_size_t;

#else

#include <mach/mach_vm.h>
#define PL_HAVE_MACH_VM 1

/** The largest address value that can be represented via the pl_vm_address_t type. */
#define PL_VM_ADDRESS_MAX UINT64_MAX

/** The largest address value that can be represented via the pl_vm_size_t type. */
#define PL_VM_SIZE_MAX UINT64_MAX

/** Architecture-independent VM address type.
 * @ingroup plcrash_async */
typedef mach_vm_address_t pl_vm_address_t;

/** Architecture-independent VM size type. 
 * @ingroup plcrash_async */
typedef mach_vm_size_t pl_vm_size_t;

#endif /* TARGET_OS_IPHONE */


// assert() support. We prefer to leave assertions on in release builds, but need
// to disable them in async-safe code paths.
#ifdef PLCF_RELEASE_BUILD

#define PLCF_ASSERT(expr)

#else

#define PLCF_ASSERT(expr) assert(expr)

#endif /* PLCF_RELEASE_BUILD */


// Debug output support. Lines are capped at 128 (stack space is scarce). This implemention
// is not async-safe and should not be enabled in release builds
#ifdef PLCF_RELEASE_BUILD

#define PLCF_DEBUG(msg, args...)

#else

#define PLCF_DEBUG(msg, args...) {\
    char __tmp_output[128];\
    snprintf(__tmp_output, sizeof(__tmp_output), "[PLCrashReport] "); \
    plcrash_async_writen(STDERR_FILENO, __tmp_output, strlen(__tmp_output));\
    \
    snprintf(__tmp_output, sizeof(__tmp_output), ":%d: ", __LINE__); \
    plcrash_async_writen(STDERR_FILENO, __func__, strlen(__func__));\
    plcrash_async_writen(STDERR_FILENO, __tmp_output, strlen(__tmp_output));\
    \
    snprintf(__tmp_output, sizeof(__tmp_output), msg, ## args); \
    plcrash_async_writen(STDERR_FILENO, __tmp_output, strlen(__tmp_output));\
    \
    __tmp_output[0] = '\n'; \
    plcrash_async_writen(STDERR_FILENO, __tmp_output, 1); \
}

#endif /* PLCF_RELEASE_BUILD */


/**
 * @ingroup plcrash_async
 * Error return codes.
 */
typedef enum  {
    /** Success */
    PLCRASH_ESUCCESS = 0,
    
    /** Unknown error (if found, is a bug) */
    PLCRASH_EUNKNOWN,
    
    /** The output file can not be opened or written to */
    PLCRASH_OUTPUT_ERR,
    
    /** No memory available (allocation failed) */
    PLCRASH_ENOMEM,
    
    /** Unsupported operation */
    PLCRASH_ENOTSUP,
    
    /** Invalid argument */
    PLCRASH_EINVAL,
    
    /** Internal error */
    PLCRASH_EINTERNAL,

    /** Access to the specified resource is denied. */
    PLCRASH_EACCESS,

    /** The requested resource could not be found. */
    PLCRASH_ENOTFOUND,
} plcrash_error_t;

const char *plcrash_async_strerror (plcrash_error_t error);

kern_return_t plcrash_async_read_addr (mach_port_t task, pl_vm_address_t source, void *dest, pl_vm_size_t len);

int plcrash_async_strncmp(const char *s1, const char *s2, size_t n);
void *plcrash_async_memcpy(void *dest, const void *source, size_t n);

ssize_t plcrash_async_writen (int fd, const void *data, size_t len);

/**
 * @internal
 * @ingroup plcrash_async_bufio
 *
 * Async-safe buffered file output. This implementation is only intended for use
 * within signal handler execution of crash log output.
 */
typedef struct plcrash_async_file {
    /** Output file descriptor */
    int fd;

    /** Output limit */
    off_t limit_bytes;

    /** Total bytes written */
    off_t total_bytes;

    /** Current length of data in buffer */
    size_t buflen;

    /** Buffered output */
    char buffer[256];
} plcrash_async_file_t;


void plcrash_async_file_init (plcrash_async_file_t *file, int fd, off_t output_limit);
bool plcrash_async_file_write (plcrash_async_file_t *file, const void *data, size_t len);
bool plcrash_async_file_flush (plcrash_async_file_t *file);
bool plcrash_async_file_close (plcrash_async_file_t *file);

#endif /* PLCRASH_ASYNC_H */
