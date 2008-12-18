/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#include "PLCrashAsyncSignalInfo.h"

#import <unistd.h>
#import <signal.h>

/**
 * @ingroup plcrash_async_signal_info
 * @{
 */

struct signal_code {
    int signal;
    int si_code;
    const char *name;
};

#if __APPLE__
struct signal_code signal_codes[] = {
    /* SIGILL */
    { SIGILL,   ILL_NOOP,       "ILL_NOOP"    },
    { SIGILL,   ILL_ILLOPC,     "ILL_ILLOPC"  },
    { SIGILL,   ILL_ILLTRP,     "ILL_ILLTRP"  },
    { SIGILL,   ILL_PRVOPC,     "ILL_PRVOPC"  },
    { SIGILL,   ILL_ILLOPN,     "ILL_ILLOPN"  },
    { SIGILL,   ILL_ILLADR,     "ILL_ILLADR"  },
    { SIGILL,   ILL_PRVREG,     "ILL_PRVREG"  },
    { SIGILL,   ILL_COPROC,     "ILL_COPROC"  },
    { SIGILL,   ILL_BADSTK,     "ILL_BADSTK"  },

    /* SIGFPE */
    { SIGFPE,   FPE_NOOP,       "FPE_NOOP"    },
    { SIGFPE,   FPE_FLTDIV,     "FPE_FLTDIV"  },
    { SIGFPE,   FPE_FLTOVF,     "FPE_FLTOVF"  },
    { SIGFPE,   FPE_FLTUND,     "FPE_FLTUND"  },
    { SIGFPE,   FPE_FLTRES,     "FPE_FLTRES"  },
    { SIGFPE,   FPE_FLTINV,     "FPE_FLTINV"  },
    { SIGFPE,   FPE_FLTSUB,     "FPE_FLTSUB"  },
    { SIGFPE,   FPE_INTDIV,     "FPE_INTDIV"  },
    { SIGFPE,   FPE_INTOVF,     "FPE_INTOVF"  },

    /* SIGSEGV */
    { SIGSEGV,  SEGV_NOOP,      "SEGV_NOOP"   },
    { SIGSEGV,  SEGV_MAPERR,    "SEGV_MAPERR" },
    { SIGSEGV,  SEGV_ACCERR,    "SEGV_ACCERR" },

    /* SIGBUS */
    { SIGBUS,   BUS_NOOP,       "BUS_NOOP"    },
    { SIGBUS,   BUS_ADRALN,     "BUS_ADRALN"  },
    { SIGBUS,   BUS_ADRERR,     "BUS_ADRERR"  },
    { SIGBUS,   BUS_OBJERR,     "BUS_OBJERR"  },

    /* SIGTRAP */
    { SIGTRAP,  TRAP_BRKPT,     "TRAP_BRKPT"  },
    { SIGTRAP,  TRAP_TRACE,     "TRAP_TRACE"  },

    { 0, 0, NULL }
};
#else
#error Unsupported Platform
#endif

/**
 * @internal
 *
 * Map a signal code to a signal name, or return NULL if no
 * mapping is available.
 */
const char *plcrash_async_signal_sigcode (int signal, int si_code) {
    for (int i = 0; signal_codes[i].name != NULL; i++) {
        /* Check for match */
        if (signal_codes[i].signal == signal && signal_codes[i].si_code == si_code)
            return signal_codes[i].name;
    }

    /* No match */
    return NULL;
}

/**
 * @} plcrash_async_signal_info
 */