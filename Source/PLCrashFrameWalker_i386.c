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

#import "PLCrashFrameWalker.h"
#import "PLCrashAsync.h"

#import <signal.h>
#import <assert.h>
#import <stdlib.h>

#define RETGEN(name, type, ts, result) {\
    *result = (ts->x86_state. type . __ ## name); \
    return PLFRAME_ESUCCESS; \
}

#ifdef __i386__

// PLFrameWalker API
plframe_error_t plframe_get_reg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_greg_t *reg) {
    plframe_thread_state_t *ts = &cursor->thread_state;

    /* Supported register for this context state? */
    if (cursor->fp[0] != NULL) {
        if (regnum == PLFRAME_X86_EIP) {
            *reg = (plframe_greg_t) cursor->fp[1];
            return PLFRAME_ESUCCESS;
        }
        
        return PLFRAME_ENOTSUP;
    }

    /* All word-sized registers */
    switch (regnum) {
        case PLFRAME_X86_EAX:
            RETGEN(eax, thread.uts.ts32, ts, reg);
    
        case PLFRAME_X86_EDX:
            RETGEN(edx, thread.uts.ts32, ts, reg);
            
        case PLFRAME_X86_ECX:
            RETGEN(ecx, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_EBX:
            RETGEN(ebx, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_EBP:
            RETGEN(ebp, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_ESI:
            RETGEN(esi, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_EDI:
            RETGEN(edi, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_ESP:
            RETGEN(esp, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_EIP:
            RETGEN(eip, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_EFLAGS:
            RETGEN(eflags, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_TRAPNO:
            RETGEN(trapno, exception.ues.es32, ts, reg);

        case PLFRAME_X86_CS:
            RETGEN(cs, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_DS:
            RETGEN(ds, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_ES:
            RETGEN(es, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_FS:
            RETGEN(fs, thread.uts.ts32, ts, reg);

        case PLFRAME_X86_GS:
            RETGEN(gs, thread.uts.ts32, ts, reg);

        default:
            // Unsupported register
            return PLFRAME_EBADREG;
    }

    /* Shouldn't be reachable */
    return PLFRAME_EUNKNOWN;
}

// PLFrameWalker API
plframe_error_t plframe_get_freg (plframe_cursor_t *cursor, plframe_regnum_t regnum, plframe_fpreg_t *fpreg) {
    return PLFRAME_ENOTSUP;
}

// PLFrameWalker API
const char *plframe_get_regname (plframe_regnum_t regnum) {
    /* All word-sized registers */
    switch (regnum) {
        case PLFRAME_X86_EAX:
            return "eax";
            
        case PLFRAME_X86_EDX:
            return "edx";
            
        case PLFRAME_X86_ECX:
            return "ecx";
            
        case PLFRAME_X86_EBX:
            return "ebx";
            
        case PLFRAME_X86_EBP:
            return "ebp";
            
        case PLFRAME_X86_ESI:
            return "esi";
            
        case PLFRAME_X86_EDI:
            return "edi";
            
        case PLFRAME_X86_ESP:
            return "esp";
            
        case PLFRAME_X86_EIP:
            return "eip";
            
        case PLFRAME_X86_EFLAGS:
            return "eflags";
            
        case PLFRAME_X86_TRAPNO:
            return "trapno";
            
        case PLFRAME_X86_CS:
            return "cs";
            
        case PLFRAME_X86_DS:
            return "ds";
            
        case PLFRAME_X86_ES:
            return "es";
            
        case PLFRAME_X86_FS:
            return "fs";
            
        case PLFRAME_X86_GS:
            return "gs";
            
        default:
            // Unsupported register
            break;
    }
   
    /* Unsupported register is an implementation error (checked in unit tests) */
    PLCF_DEBUG("Missing register name for register id: %d", regnum);
    abort();
}

#endif
