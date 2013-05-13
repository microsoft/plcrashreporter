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

#ifndef PLCRASH_FRAME_COMPACTUNWIND_H
#define PLCRASH_FRAME_COMPACTUNWIND_H

#include "PLCrashFrameWalker.h"
#include "PLCrashAsyncCompactUnwindEncoding.h"

plframe_error_t plframe_cursor_apply_compact_unwind (task_t task,
                                                     const plcrash_async_thread_state_t *thread_state,
                                                     plcrash_async_cfe_entry_t *entry,
                                                     plcrash_async_thread_state_t *new_thread_state);

plframe_error_t plframe_cursor_read_compact_unwind (task_t task,
                                                    plcrash_async_image_list_t *image_list,
                                                    const plframe_stackframe_t *current_frame,
                                                    const plframe_stackframe_t *previous_frame,
                                                    plframe_stackframe_t *next_frame);

#endif /* PLCRASH_FRAME_COMPACTUNWIND_H */
