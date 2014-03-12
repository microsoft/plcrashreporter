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

#include "PLCrashAsyncFile.hpp"

#include "SecureRandom.hpp"

#include <unistd.h>
#include <errno.h>

using namespace plcrash::async;

/**
 * @internal
 * @ingroup plcrash_async
 * @{
 */

/**
 * Write @a len bytes to fd, looping until all bytes are written
 * or an error occurs.
 *
 * For the local file system, only one call to write() should be necessary.
 *
 * @param fd Open, writable file descriptor.
 * @param data The buffer to be written to @a fd.
 * @param len The total size of @a data, in bytes.
 *
 * @return Returns @len on success, or -1 on failure.
 */
ssize_t AsyncFile::writen (int fd, const void *data, size_t len) {
    const uint8_t *p;
    size_t left;
    ssize_t written = 0;
    
    /* Loop until all bytes are written */
    p = (const uint8_t *) data;
    left = len;
    while (left > 0) {
        if ((written = ::write(fd, p, left)) <= 0) {
            if (errno == EINTR) {
                // Try again
                written = 0;
            } else {
                return -1;
            }
        }
        
        left -= written;
        p += written;
    }
    
    return len - left;
}

/**
 * Read @a len bytes from fd, looping until all bytes are read or an error occurs.
 *
 * @param fd Open, readable file descriptor.
 * @param data The buffer containing the bytes read from @a fd.
 * @param len The total number of bytes to read.
 *
 * @return Returns @len on success, or -1 on failure.
 */
ssize_t AsyncFile::readn (int fd, void *data, size_t len) {
    uint8_t *p;
    size_t left;
    ssize_t bytes_read = 0;
    
    /* Loop until all bytes are read */
    p = (uint8_t *) data;
    left = len;
    while (left > 0) {
        if ((bytes_read = ::read(fd, p, left)) <= 0) {
            if (errno == EINTR) {
                // Try again
                bytes_read = 0;
            } else {
                return -1;
            }
        }
        
        left -= bytes_read;
        p += bytes_read;
    }
    
    return len - left;
}

/**
 * Replace any trailing 'X' characters in @a pathTemplate, create the file at that path, and return a file
 * descriptor open for reading and writing.
 *
 * @param pathTemplate A writable template path. This template will be modified to contain the actual file path.
 * @param mode The file mode for the target file. The file will be created with this mode, modified by the process' umask value.
 * @param outfd On success, a file descriptor opened for both reading and writing.
 *
 * @return PLCRASH_ESUCCESS if the temporary file was successfully opened, or an error if the target path can not be opened.
 *
 * @warning While this method is a loose analogue of libc mkstemp(3), the semantics differ, and API clients should not
 * rely on any similarities with the standard library function that are not explicitly documented.
 */
plcrash_error_t AsyncFile::mktemp (char *ptemplate, mode_t mode, int *outfd) {
    /* Characters to use for padding. We don't use a-z characters, as HFS+ is case insensitive by default. */
    static const char padchar[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    /* Find the start and length of the suffix ('X') */
    size_t suffix_i = 0;
    size_t suffix_len = 0;

    /* Find the suffix length and position */
    for (size_t i = 0; ptemplate[i] != '\0'; i++) {
        /* Walk candidate suffix string, verifying that it occurs at the end of the string. */
        if (ptemplate[i] == 'X') {
            /* Record the suffix position */
            suffix_i = i;

            /* Search for the end of the suffix string */
            while (ptemplate[i] == 'X') {
                suffix_len++;
                i++;
            }
            
            /* If the candidate suffix didn't end at the end of the string, this wasn't actually the suffix */
            if (ptemplate[i] != '\0') {
                suffix_len = 0;
                suffix_i = 0;
            }
        }
    }
    
    /* Insert random suffix into the template. */
    SecureRandom rnd = SecureRandom();
    for (size_t i = 0; i < suffix_len; i++) {
        plcrash_error_t err;
        uint32_t charIndex;
        
        /* Try to read a random suffix character */
        if ((err = rnd.uniform(sizeof(padchar) - 1, &charIndex)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to fetch bytes from SecureRandom.uniform(): %s", plcrash_async_strerror(err));
            return err;
        }
        
        /* Update the suffix. */
        PLCF_ASSERT(charIndex < sizeof(padchar));
        ptemplate[suffix_i + i] = padchar[charIndex];
    }

    /* Save a copy of the suffix. We use this to determine when roll-over occurs while trying all possible combinations. */
    char original_suffix[suffix_len];
    plcrash_async_memcpy(original_suffix, &ptemplate[suffix_i], suffix_len);
    
    /* Recursion state; we use this to track completion at each recursion depth; a state of '\0' signifies incomplete, whereas
     * any other character signifies completion. */
    char recursion_state[suffix_len];
    plcrash_async_memset(recursion_state, '\0', sizeof(recursion_state));
    
    /*
     * Loop until open(2) either succeeds, returns an error other than EEXIST, or we've tried every available permutation.
     */
    size_t depth = 0;
    while (true) {
        /* Try to open the file. */
        *outfd = open(ptemplate, O_CREAT|O_EXCL|O_RDWR, mode);
        
        /* Terminate on success */
        if (*outfd >= 0)
            return PLCRASH_ESUCCESS;
        
        /* Terminate if we get an error other than EEXIST */
        if (errno != EEXIST) {
            PLCF_DEBUG("Failed to open output file '%s' in mktemp(): %d", ptemplate, errno);
            return PLCRASH_OUTPUT_ERR;
        }
        
        /* Determine the next padding character to be added */
        char pchar;
        while (true) {
            PLCF_ASSERT(depth < suffix_len);

            /* Locate the pad_idx for this depth. */
            size_t pad_idx;
            for (pad_idx = 0; padchar[pad_idx] != ptemplate[suffix_i + depth] && padchar[pad_idx] != '\0'; pad_idx++);
            
            /* Fetch the next padding character, wrapping around if we hit the end. */
            PLCF_ASSERT(pad_idx < sizeof(padchar));
            pad_idx++;
            if (padchar[pad_idx] == '\0')
                pad_idx = 0;
            
            pchar = padchar[pad_idx];
            
            /* If we've tried all values at this template position, we need to backtrack. */
            if (pchar == original_suffix[depth]) {
                /* Record the completion state */
                recursion_state[depth] = pchar;

                /* Backtrack until we hit an incomplete value, or the first entry */
                while (depth > 0 && recursion_state[depth] != '\0') {
                    recursion_state[depth] = '\0';
                    
                    /* Reset the starting state. */
                    if (depth > 0)
                        ptemplate[suffix_i + depth] = pchar;

                    depth--;
                    PLCF_ASSERT(depth < suffix_len);
                }

                
                /* Check for completion */
                if (depth == 0 && recursion_state[depth] != '\0') {
                    PLCF_DEBUG("Tried all possible temporary file names for '%s'; could not find an available temporary name.", ptemplate);
                    return PLCRASH_OUTPUT_ERR;
                }
                
                continue;
            }

            /* Update the suffix */
            ptemplate[suffix_i + depth] = pchar;

            /* Recurse if we can, retry open() if we can't */
            if (depth + 1 == suffix_len) {
                /* Retry! */
                break;
            } else {
                /* Recurse! */
                depth++;
            }
            break;
        }
    }

    /* Unreachable! */
    abort();
}

/**
 * Construct a new AsyncFile instance.
 *
 * @param fd Open, writable file descriptor.
 * @param output_limit Maximum number of bytes that will be written to disk. Intended as a
 * safety measure prevent a run-away crash log writer from filling the disk. Specify
 * 0 to disable any limits. Once the limit is reached, all data will be dropped.
 */
AsyncFile::AsyncFile (int fd, off_t output_limit) {
    this->fd = fd;
    this->buflen = 0;
    this->total_bytes = 0;
    this->limit_bytes = output_limit;
}

/**
 * @internal
 *
 * Return the size of the internal buffer. This is the maximum number of bytes
 * that will be buffered before AsyncFile::write flushes the buffer contents
 * to disk.
 *
 * @warning This method is intended to be used by unit tests when determining
 * the minimum amount of data that must be written to trigger a buffer flush,
 * and should not be used externally.
 */
size_t AsyncFile::buffer_size (void) {
    return sizeof(this->buffer);
}

/**
 * Write all bytes from @a data to the file buffer. Returns true on success,
 * or false if an error occurs.
 *
 * @param data The buffer to be written to @a fd.
 * @param len The total size of @a data, in bytes.
 */
bool AsyncFile::write (const void *data, size_t len) {
    /* Check and update output limit */
    if (this->limit_bytes != 0 && len + this->total_bytes > this->limit_bytes) {
        return false;
    } else if (this->limit_bytes != 0) {
        this->total_bytes += len;
    }
    
    /* Check if the buffer will fill */
    if (this->buflen + len > sizeof(this->buffer)) {
        /* Flush the buffer */
        if (AsyncFile::writen(this->fd, this->buffer, this->buflen) < 0) {
            PLCF_DEBUG("Error occured writing to crash log: %s", strerror(errno));
            return false;
        }
        
        this->buflen = 0;
    }
    
    /* Check if the new data fits within the buffer, if so, buffer it */
    if (len + this->buflen <= sizeof(this->buffer)) {
        plcrash_async_memcpy(this->buffer + this->buflen, data, len);
        this->buflen += len;
        
        return true;
        
    } else {
        /* Won't fit in the buffer, just write it */
        if (AsyncFile::writen(this->fd, data, len) < 0) {
            PLCF_DEBUG("Error occured writing to crash log: %s", strerror(errno));
            return false;
        }
        
        return true;
    }
}


/**
 * Flush all buffered bytes from the file buffer to disk.
 */
bool AsyncFile::flush (void) {
    /* Anything to do? */
    if (this->buflen == 0)
        return true;
    
    /* Write remaining */
    if (AsyncFile::writen(this->fd, this->buffer, this->buflen) < 0) {
        PLCF_DEBUG("Error occured writing to crash log: %s", strerror(errno));
        return false;
    }
    
    this->buflen = 0;
    
    return true;
}


/**
 * Close the backing file descriptor.
 */
bool AsyncFile::close (void) {
    /* Flush any pending data */
    if (!this->flush())
        return false;
    
    /* Close the file descriptor */
    if (::close(this->fd) != 0) {
        PLCF_DEBUG("Error closing file: %s", strerror(errno));
        return false;
    }
    
    return true;
}

/**
 * @} plcrash_async
 */
