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

#include "PLCrashFrameStackUnwind.h"
#include "PLCrashAsync.h"

/**
 * Fetch the next frame.
 *
 * @param cursor A cursor instance initialized with plframe_cursor_init();
 * @return Returns PLFRAME_ESUCCESS on success, PLFRAME_ENOFRAME is no additional frames are available, or a standard plframe_error_t code if an error occurs.
 */
plframe_error_t plframe_cursor_next_fp (plframe_cursor_t *cursor) {
    kern_return_t kr;
    void *prevfp = cursor->fp[0];
    
    /* Fetch the next stack address */
    if (cursor->init_frame) {
        /* The first frame is already available, so there's nothing to do */
        cursor->init_frame = false;
        return PLFRAME_ESUCCESS;
    } else {
        if (cursor->fp[0] == NULL) {
            /* No frame data has been loaded, fetch it from register state */
            plframe_greg_t nfp;
            plframe_error_t err;
            
            if ((err = plframe_cursor_get_reg(cursor, PLFRAME_REG_FP, &nfp)) != PLFRAME_ESUCCESS) {
                /* This should never fail, as it is called only at the top of the stack, where thread state is
                 * available. */
                PLCF_DEBUG("Unexpected error fetching frame pointer: %s", plframe_strerror(err));
                return PLFRAME_EUNKNOWN;
            }
            
            kr = plcrash_async_read_addr(cursor->task, nfp, cursor->fp, sizeof(cursor->fp));
        } else {
            /* Frame data loaded, walk the stack */
            kr = plcrash_async_read_addr(cursor->task, (pl_vm_address_t) cursor->fp[0], cursor->fp, sizeof(cursor->fp));
        }
    }
    
    /* Was the read successful? */
    if (kr != KERN_SUCCESS)
        return PLFRAME_EBADFRAME;
    
    /* Check for completion */
    if (cursor->fp[0] == NULL)
        return PLFRAME_ENOFRAME;
    
    // TODO - Extend the platform definition to handle this.
#if defined(__arm__) || defined(__i386__) || defined(__x86_64__)
    /* Is the stack growing in the right direction? */
    if (!cursor->init_frame && prevfp > cursor->fp[0])
        return PLFRAME_EBADFRAME;
#else
#error Add platform support
#endif
    
    /* New frame fetched */
    return PLFRAME_ESUCCESS;
}