/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2008-2014 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_FILE_H
#define PLCRASH_ASYNC_FILE_H 1

#include "PLCrashAsync.h"
#include <fcntl.h>

namespace plcrash { namespace async {
    
/**
 * @internal
 * @ingroup plcrash_async
 * @{
 */

/**
 * @internal
 *
 * Async-safe buffered file output. This implementation is only intended for use
 * within signal handler execution of crash log output.
 */
class AsyncFile {
public:    
    static ssize_t writen (int fd, const void *data, size_t len);
    static ssize_t readn (int fd, void *data, size_t len);

    static plcrash_error_t mktemp (char *ptemplate, mode_t mode, int *outfd);

    AsyncFile (int fd, off_t output_limit);

    bool write (const void *data, size_t len);
    bool flush (void);
    bool close (void);

    size_t buffer_size (void);

private:
    /** Output file descriptor */
    int _fd;
    
    /** Output limit */
    off_t _limit_bytes;
    
    /** Total bytes written */
    off_t _total_bytes;
    
    /** Current length of data in buffer */
    size_t _buflen;
    
    /** Buffered output */
    char _buffer[256];
};
    
}}

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_FILE_H */
