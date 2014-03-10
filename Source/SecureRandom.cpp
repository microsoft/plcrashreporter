/*
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

#include "SecureRandom.hpp"
#include "PLCrashAsyncFile.hpp"

#include <fcntl.h>
#include <errno.h>
#include <libkern/OSAtomic.h>
#include <unistd.h>

using namespace plcrash::async;

/**
 * @internal
 * @ingroup plcrash_async
 * @{
 */

/**
 * Construct a new SecureRandom instance.
 */
SecureRandom::SecureRandom () {
    /* Try opening the file */
    _random_fd = open("/dev/random", O_RDONLY);
    if (_random_fd == -1) {
        /* This should not fail on any sane system */
        PLCF_DEBUG("Unexpected error opening /dev/random: %d", errno);
    }
}


/* SecureRandom destructor */
SecureRandom::~SecureRandom () {
    if (_random_fd != -1) {
        if (close(_random_fd) != 0) {
            /* This should not fail on any sane system */
            PLCF_DEBUG("Unexpected error in close() on /dev/random: %d", errno);
        }
    }
}

/**
 * Read @a count of cryptographically secure random bytes.
 *
 * @param bytes The output buffer to which the data will be written.
 * @param count The number of random bytes to return in @a bytes.
 *
 * @return Returns PLCRASH_ESUCCESS on success, or an error if reading from
 * the random number source fails.
 */
plcrash_error_t SecureRandom::readBytes (void *bytes, size_t count) {
    /* Ensure that the random device is accessible */
    if (_random_fd == -1)
        return PLCRASH_EINTERNAL;

    /* Read the requested number of bytes. */
    if (AsyncFile::readn(_random_fd, bytes, count) == -1) {
        /* This should not fail on any sane system */
        PLCF_DEBUG("Unexpected error in read() on /dev/random: %d", errno);
        return PLCRASH_EINTERNAL;
    }

    return PLCRASH_ESUCCESS;
}

/**
 * @}
 */
