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

/** Characters to use for the suffix in mktemp(). We don't use a-z characters, as HFS+ is case-insensitive by default. */
static const char mktemp_padchar[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/**
 * Given a mktemp() padding character, return the index position of that character. Will raise an assertion
 * if an invalid padding character is passed.
 */
static size_t mktemp_padchar_index (char current_char) {
    /* Locate the pad_idx for this target. */
    size_t idx;
    for (idx = 0; mktemp_padchar[idx] != current_char; idx++) {
        /* Can only fire if an invalid character is passed to this function. */
        PLCF_ASSERT(mktemp_padchar[idx] != '\0');
    }
    
    return idx;
}

/**
 * Given a mktemp() padding character, return the next character in the alphabet. Will raise an assertion
 * if an invalid padding character is passed.
 */
static char mktemp_padchar_next (char current_char) {
    size_t idx = mktemp_padchar_index(current_char);
    
    /* Fetch the next padding character, wrapping around if we hit the end. */
    PLCF_ASSERT(idx < sizeof(mktemp_padchar));
    idx++;
    if (mktemp_padchar[idx] == '\0')
        idx = 0;

    /* Return the result */
    return mktemp_padchar[idx];
}

/**
 * Given a mktemp() padding character, return the previous character in the alphabet. Will raise an assertion
 * if an invalid padding character is passed.
 */
static char mktemp_padchar_prev (char current_char) {
    size_t idx = mktemp_padchar_index(current_char);
    
    /* Fetch the previous padding character, wrapping around if we hit the beginning. */
    PLCF_ASSERT(idx < sizeof(mktemp_padchar));
    if (idx == 0) {
        idx = sizeof(mktemp_padchar) - 2; /* Skip trailing NUL */
    } else {
        idx--;
    }
    
    /* Return the result */
    return mktemp_padchar[idx];
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
plcrash_error_t AsyncFile::mktemp (char *pathTemplate, mode_t mode, int *outfd) {
    /* Fetch the template string length, not including trailing NULL */
    size_t ptemplate_len = strlen(pathTemplate);
    
    /* Find the start and length of the suffix ('X') */
    size_t suffix_i = 0;
    size_t suffix_len = 0;

    /* Find the suffix length and position */
    for (size_t i = 0; i < ptemplate_len; i++) {
        /* Walk candidate suffix string, verifying that it occurs at the end of the string. */
        if (pathTemplate[i] == 'X') {
            /* Record the suffix position */
            suffix_i = i;

            /* Search for the end of the suffix string */
            while (pathTemplate[i] == 'X') {
                suffix_len++;
                i++;
                PLCF_ASSERT(i < ptemplate_len+1);
            }
            
            /* If the candidate suffix didn't end at the end of the string, this wasn't actually the suffix; allow
             * the loop to continue. */
            if (pathTemplate[i] != '\0') {
                suffix_len = 0;
                suffix_i = 0;
            }
        }
    }

    /* If no suffix is found, set the suffix index to the empty end of the template */
    if (suffix_i == 0 && pathTemplate[suffix_i] != 'X')
        suffix_i = strlen(pathTemplate);
    
    /* Assert that the computed suffix length matches the actual NUL-terminated string length */
    PLCF_ASSERT(strlen(&pathTemplate[suffix_i]) == suffix_len);
    
    /* Insert random suffix into the template. */
    SecureRandom rnd = SecureRandom();
    for (size_t i = 0; i < suffix_len; i++) {
        plcrash_error_t err;
        uint32_t charIndex;
        
        /* Try to read a random suffix character */
        if ((err = rnd.uniform(sizeof(mktemp_padchar) - 1, &charIndex)) != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Failed to fetch bytes from SecureRandom.uniform(): %s", plcrash_async_strerror(err));
            return err;
        }
        
        /* Update the suffix. */
        PLCF_ASSERT(charIndex < sizeof(mktemp_padchar));
        PLCF_ASSERT(suffix_i + i < ptemplate_len);
        pathTemplate[suffix_i + i] = mktemp_padchar[charIndex];
    }

    /* Save a copy of the suffix. We use this to determine when roll-over occurs while trying all possible combinations. */
    char original_suffix[suffix_len];
    plcrash_async_memcpy(original_suffix, &pathTemplate[suffix_i], suffix_len);
    
    /* Recursion state; this tracks the 'last' position in the alphabet for all suffix positions. This varies based on the
     * starting position. */
    char last_alphabet_suffix[suffix_len];
    for (size_t depth = 0; depth < suffix_len; depth++) {
        last_alphabet_suffix[depth] = mktemp_padchar_prev(original_suffix[depth]);
    }
    
    /*
     * Loop until open(2) either succeeds, returns an error other than EEXIST, or we've tried every available permutation.
     */
    while (true) {
        /* Try to open the file. */
        *outfd = open(pathTemplate, O_CREAT|O_EXCL|O_RDWR, mode);
        
        /* Terminate on success */
        if (*outfd >= 0)
            return PLCRASH_ESUCCESS;
        
        /* Terminate if we get an error other than EEXIST */
        if (errno != EEXIST) {
            PLCF_DEBUG("Failed to open output file '%s' in mktemp(): %d", pathTemplate, errno);
            return PLCRASH_OUTPUT_ERR;
        }

        /* 1. Find the last character in the suffix that has not been iterated to completion */
        size_t target_pos = 0;
        bool target_pos_found = false;
        for (size_t depth = 0; depth < suffix_len; depth++) {
            /* If found, save the position, and then keep looking */
            PLCF_ASSERT(suffix_i + depth < ptemplate_len);
            if (pathTemplate[suffix_i + depth] != last_alphabet_suffix[depth]) {
                target_pos = depth;
                target_pos_found = true;
            }
        }

        /* 2. Increment the target found in step #1; this is the right-most target that has not yet been fully iterated. If nothing was found, we're done. */
        if (target_pos_found) {
            PLCF_ASSERT(suffix_i + target_pos < ptemplate_len);
            
            char pchar = mktemp_padchar_next(pathTemplate[suffix_i + target_pos]);
            pathTemplate[suffix_i + target_pos] = pchar;
        } else {
            PLCF_DEBUG("Tried all possible combinations of '%s'", pathTemplate);
            return PLCRASH_OUTPUT_ERR;
        }

        /* 3. Reset all characters to the right of the target back to their initial alphabet state so that we
         * can restart iteration. */
        for (size_t depth = target_pos+1; depth < suffix_len; depth++) {
            PLCF_ASSERT(suffix_i + depth < ptemplate_len);
            pathTemplate[suffix_i + depth] = original_suffix[depth];
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
AsyncFile::AsyncFile (int fd, off_t output_limit) : _fd(fd), _limit_bytes(output_limit), _total_bytes(0), _buflen(0) {}

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
    return sizeof(_buffer);
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
    if (_limit_bytes != 0 && len + _total_bytes > _limit_bytes) {
        return false;
    } else if (_limit_bytes != 0) {
        _total_bytes += len;
    }
    
    /* Check if the buffer will fill */
    if (_buflen + len > sizeof(_buffer)) {
        /* Flush the buffer */
        if (AsyncFile::writen(_fd, _buffer, _buflen) < 0) {
            PLCF_DEBUG("Error occured writing to crash log: %s", strerror(errno));
            return false;
        }
        
        _buflen = 0;
    }
    
    /* Check if the new data fits within the buffer, if so, buffer it */
    if (len + _buflen <= sizeof(_buffer)) {
        plcrash_async_memcpy(_buffer + _buflen, data, len);
        _buflen += len;
        
        return true;
        
    } else {
        /* Won't fit in the buffer, just write it */
        if (AsyncFile::writen(_fd, data, len) < 0) {
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
    if (_buflen == 0)
        return true;
    
    /* Write remaining */
    if (AsyncFile::writen(_fd, _buffer, _buflen) < 0) {
        PLCF_DEBUG("Error occured writing to crash log: %s", strerror(errno));
        return false;
    }
    
    _buflen = 0;
    
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
    if (::close(_fd) != 0) {
        PLCF_DEBUG("Error closing file: %s", strerror(errno));
        return false;
    }
    
    return true;
}

/**
 * @} plcrash_async
 */
