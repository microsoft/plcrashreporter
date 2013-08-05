/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
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

#include "PLCrashSysctl.h"
#include <errno.h>

/**
 * @internal
 * @defgroup plcrash_host Host and Process Info
 * @ingroup plcrash_internal
 *
 * Implements general utility functions for gathering host/process statistics.
 * @{
 */

/*
 * Wrap sysctl(), automatically allocating a sufficiently large buffer for the returned data. The new buffer's
 * length will be returned in @a length.
 *
 * @param name The sysctl MIB name.
 * @param length On success, will be populated with the length of the result. If NULL, length will not be supplied.
 *
 * @return Returns a malloc-allocated buffer containing the sysctl result on success. On failure, NULL is returned
 * and the global variable errno is set to indicate the error. The caller is responsible for free()'ing the returned
 * buffer.
 */
static void *plcrash_sysctl_malloc (const char *name, size_t *length) {
    /* Attempt to fetch the data, looping until our buffer is sufficiently sized. */
    void *result = NULL;
    size_t result_len = 0;
    int ret;
    
    /* If our buffer is too small after allocation, loop until it succeeds -- the requested destination size
     * may change after each iteration. */
    do {
        /* Fetch the expected length */
        if ((ret = sysctlbyname(name, NULL, &result_len, NULL, 0)) == -1)
            break;
        
        /* Allocate the destination buffer */
        if (result != NULL)
            free(result);
        result = malloc(result_len);
        
        /* Fetch the value */
        ret = sysctlbyname(name, result, &result_len, NULL, 0);
    } while (ret == -1 && errno == ENOMEM);
    
    /* Handle failure */
    if (ret == -1) {
        int saved_errno = errno;
        
        if (result != NULL)
            free(result);
        
        errno = saved_errno;
        return NULL;
    }
    
    /* Provide the length */
    if (length != NULL)
        *length = result_len;
    
    return result;
}

/**
 * Wrap sysctl() and fetch a C string, automatically allocating a sufficiently large buffer for the returned data.
 *
 * @param name The sysctl MIB name.
 * @param length On success, will be populated with the length of the result. If NULL, length will not be supplied.
 *
 * @return Returns a malloc-allocated NULL-terminated C string containing the sysctl result on success. On failure,
 * NULL is returned and the global variable errno is set to indicate the error. The caller is responsible for
 * free()'ing the returned buffer.
 */
char *plcrash_sysctl_string (const char *name) {
    return plcrash_sysctl_malloc(name, NULL);
}

/**
 * Wrap sysctl() and fetch an integer value.
 *
 * @param name The sysctl MIB name.
 * @param result On success, the integer result will be provided via this pointer.
 *
 * @return Returns true on success. On failure, false is returned and the global variable errno is set to indicate
 * the error.
 */
bool plcrash_sysctl_int (const char *name, int *result) {
    size_t len = sizeof(*result);

    if (sysctlbyname(name, result, &len, NULL, 0) != 0)
        return false;

    return true;
}

/**
 * @}
 */
