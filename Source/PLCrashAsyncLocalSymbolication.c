/*
 * Author: Mike Ash <mikeash@plausiblelabs.com>
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

#include "PLCrashAsyncLocalSymbolication.h"

#include "PLCrashAsyncObjCSection.h"


static void saveMachOAddressCB(pl_vm_address_t address, const char *name, void *ctx) {
    *(pl_vm_address_t *)ctx = address;
}

static void saveObjCAddressCB(bool isClassMethod, plcrash_async_macho_string_t *className, plcrash_async_macho_string_t *methodName, pl_vm_address_t imp, void *ctx) {
    *(pl_vm_address_t *)ctx = imp;
}

struct objc_callback_ctx {
    char *symString;
    int symStringMaxLen;
    pl_vm_address_t imp;
    bool didError;
};

static bool append_char(char *str, char c, int *cursorPtr, int limit) {
    if (*cursorPtr >= limit)
        return false;
    
    str[(*cursorPtr)++] = c;
    return true;
}

static void callbackObjCAddressCB(bool isClassMethod, plcrash_async_macho_string_t *className, plcrash_async_macho_string_t *methodName, pl_vm_address_t imp, void *ctx) {
    struct objc_callback_ctx *ctxStruct = ctx;
    
    /* Save the IMP. */
    ctxStruct->imp = imp;
    
    /* Get the string data. */
    plcrash_error_t lengthErr;
    plcrash_error_t ptrErr;
    
    pl_vm_size_t classNameLength;
    const char *classNamePtr;
    lengthErr = plcrash_async_macho_string_get_length(className, &classNameLength);
    ptrErr = plcrash_async_macho_string_get_pointer(className, &classNamePtr);
    
    if (lengthErr != PLCRASH_ESUCCESS || ptrErr != PLCRASH_ESUCCESS) {
        if (lengthErr != PLCRASH_ESUCCESS)
            PLCF_DEBUG("plcrash_async_macho_string_get_length(className) error %d", lengthErr);
        if (ptrErr != PLCRASH_ESUCCESS)
            PLCF_DEBUG("plcrash_async_macho_string_get_pointer(className) error %d", ptrErr);
        classNameLength = 5;
        classNamePtr = "ERROR";
        ctxStruct->didError = true;
    }
    
    pl_vm_size_t methodNameLength;
    const char *methodNamePtr;
    lengthErr = plcrash_async_macho_string_get_length(methodName, &methodNameLength);
    ptrErr = plcrash_async_macho_string_get_pointer(methodName, &methodNamePtr);
    
    if (lengthErr != PLCRASH_ESUCCESS || ptrErr != PLCRASH_ESUCCESS) {
        if (lengthErr != PLCRASH_ESUCCESS)
            PLCF_DEBUG("plcrash_async_macho_string_get_length(methodName) error %d", lengthErr);
        if (ptrErr != PLCRASH_ESUCCESS)
            PLCF_DEBUG("plcrash_async_macho_string_get_pointer(methodName) error %d", ptrErr);
        methodNameLength = 5;
        methodNamePtr = "ERROR";
        ctxStruct->didError = true;
    }
    
    char *symString = ctxStruct->symString;
    int maxLen = ctxStruct->symStringMaxLen;
    
    int cursor = 0;
    append_char(symString, isClassMethod ? '+' : '-', &cursor, maxLen);
    append_char(symString, '[', &cursor, maxLen);
    
    for (pl_vm_size_t i = 0; i < classNameLength; i++) {
        bool success = append_char(symString, classNamePtr[i], &cursor, maxLen);
        if (!success)
            break;
    }
    
    append_char(symString, ' ', &cursor, maxLen);
    
    for (pl_vm_size_t i = 0; i < methodNameLength; i++) {
        bool success = append_char(symString, methodNamePtr[i], &cursor, maxLen);
        if (!success)
            break;
    }
    
    append_char(symString, ']', &cursor, maxLen);
}

/**
 * Initialize a symbol-finding context object.
 *
 * @param context A pointer to the context object to initialize.
 * @return An error code.
 */
plcrash_error_t pl_async_local_find_symbol_context_init (pl_async_local_find_symbol_context_t *context) {
    return pl_async_objc_context_init(&context->objcContext);
}

/**
 * Free a symbol-finding context object.
 *
 * @param context A pointer to the context object to free.
 */
void pl_async_local_find_symbol_context_free (pl_async_local_find_symbol_context_t *context) {
    pl_async_objc_context_free(&context->objcContext);
}

plcrash_error_t pl_async_local_find_symbol(pl_async_macho_t *image, pl_async_local_find_symbol_context_t *findContext, pl_vm_address_t pc, pl_async_found_symbol_cb callback, void *ctx) {
    pl_vm_address_t machoAddress = 0;
    plcrash_error_t machoErr = pl_async_macho_find_symbol(image, pc, saveMachOAddressCB, &machoAddress);
    
    pl_vm_address_t objcAddress = 0;
    plcrash_error_t objcErr = pl_async_objc_find_method(image, &findContext->objcContext, pc, saveObjCAddressCB, &objcAddress);
    
    if (machoErr != PLCRASH_ESUCCESS && objcErr != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Could not find symbol for PC %p image %p", (void *)pc, image);
        PLCF_DEBUG("pl_async_macho_find_symbol error %d, pl_async_objc_find_method error %d", machoErr, objcErr);
        return machoErr;
    }
    
    /* Choose whichever one has the higher address (closer match), or whichever one
     * didn't error. */
    if (objcErr != PLCRASH_ESUCCESS || machoAddress > objcAddress) {
        return pl_async_macho_find_symbol(image, pc, callback, ctx);
    } else {
        char symString[128] = {};
        int maxLen = sizeof(symString) - 1;

        struct objc_callback_ctx innerCtx = {
            .symString = symString,
            .symStringMaxLen = maxLen,
            .imp = 0,
            .didError = false
        };
        plcrash_error_t err = pl_async_objc_find_method(image, &findContext->objcContext, pc, callbackObjCAddressCB, &innerCtx);
        if (err == PLCRASH_ESUCCESS && innerCtx.imp != 0 && !innerCtx.didError) {
            callback(innerCtx.imp, symString, ctx);
            return PLCRASH_ESUCCESS;
        } else {
            return pl_async_macho_find_symbol(image, pc, callback, ctx);
        }
    }
}
