/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#ifdef __i386__

// 32-bit
typedef int32_t plframe_pdef_word_t;
typedef int32_t plframe_pdef_fpreg_t;

// Stack grows down
#define PLFRAME_PDEF_STACK_DIRECTION PLFRAME_STACK_DIR_DOWN

#endif