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

#ifndef PLCRASH_ASYNC_DWARF_STACK_H
#define PLCRASH_ASYNC_DWARF_STACK_H 1

#include <cstddef>

/**
 * @internal
 * @ingroup plcrash_async_dwarf_stack
 * @{
 */

namespace plcrash {

/**
 * @internal
 *
 * A simple machine pointer stack for use with DWARF opcode/CFA evaluation.
 */
template <class machine_ptr, size_t S> class dwarf_stack {
    machine_ptr mem[S];
    machine_ptr *sp = mem;
    
public:
    inline bool push (machine_ptr value);
    inline bool pop (machine_ptr *value);
};

/**
 * Push a single value onto the stack.
 *
 * @param value The value to push.
 * @return Returns true on success, or false if the stack is full.
 */
template <class P, size_t S> inline bool dwarf_stack<P,S>::push (P value) {
    /* Refuse to exceed the allocated stack size */
    if (sp == &mem[S])
        return false;
    
    *sp = value;
    sp++;
    
    return true;
}

/**
 * Pop a single value from the stack.
 *
 * @param value An address to which the popped value will be written.
 * @return Returns true on success, or false if the stack is empty.
 */
template <class P, size_t S> inline bool dwarf_stack<P,S>::pop (P *value) {
    /* Refuse to pop the final value */
    if (sp == mem)
        return false;

    sp--;
    *value = *sp;
    return true;
}

}

/**
 * @}
 */

#endif /* PLCRASH_ASYNC_DWARF_STACK_H */
