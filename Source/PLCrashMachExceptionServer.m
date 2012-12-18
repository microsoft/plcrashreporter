/*
 * Author: Landon Fuller <landonf@bikemonkey.org>
 *
 * Copyright (c) 2012 Plausible Labs Cooperative, Inc.
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

#import "PLCrashMachExceptionServer.h"
#import "plcrash_exc_server.h"

/***
 * @internal
 * Implements Crash Reporter mach exception handling.
 */
@implementation PLCrashMachExceptionServer

kern_return_t plcrash_exception_raise (
   mach_port_t exception_port,
   mach_port_t thread,
   mach_port_t task,
   exception_type_t exception,
   exception_data_t code, mach_msg_type_number_t codeCnt)
{
    return KERN_SUCCESS;
}

kern_return_t plcrash_exception_raise_state (
   mach_port_t exception_port,
   exception_type_t exception,
   const exception_data_t code,
   mach_msg_type_number_t codeCnt,
   int *flavor,
   const thread_state_t old_state,
   mach_msg_type_number_t old_stateCnt,
   thread_state_t new_state,
   mach_msg_type_number_t *new_stateCnt)
{
    return KERN_SUCCESS;
}

kern_return_t plcrash_exception_raise_state_identity (
   mach_port_t exception_port,
   mach_port_t thread,
   mach_port_t task,
   exception_type_t exception,
   exception_data_t code,
   mach_msg_type_number_t codeCnt,
   int *flavor,
   thread_state_t old_state,
   mach_msg_type_number_t old_stateCnt,
   thread_state_t new_state,
   mach_msg_type_number_t *new_stateCnt)
{
    return KERN_SUCCESS;
}

@end
