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

#include "PLCrashAsyncObjCSection.h"


static char * const kObjCSegmentName = "__OBJC";
static char * const kDataSegmentName = "__DATA";

static char * const kObjCModuleInfoSectionName = "__module_info";
static char * const kClassListSectionName = "__objc_classlist";

struct pl_objc_module {
    uint32_t version;
    uint32_t size;
    uint32_t name;
    uint32_t symtab;
};

struct pl_objc2_class {
    uint64_t isa;
    uint64_t superclass;
    uint64_t cache;
    uint64_t vtable;
    uint64_t data_rw;
    uint64_t reserved1;
    uint64_t reserved2;
    uint64_t reserved3;
};

struct pl_objc2_class_data_rw_t {
    uint32_t flags;
    uint32_t version;
    uint64_t data_ro;
};

struct pl_objc2_class_data_ro_t {
    uint32_t flags;
    uint32_t instanceStart;
    uint32_t instanceSize;
    uint32_t reserved;
    uint64_t ivarLayout;
    uint64_t name;
    uint64_t baseMethods;
    uint64_t baseProtocols;
    uint64_t ivars;
    uint64_t weakIvarLayout;
    uint64_t baseProperties;
};


static plcrash_error_t read_string (pl_async_macho_t *image, pl_vm_address_t address, plcrash_async_mobject_t *outMobj) {
    pl_vm_address_t cursor = address;
    
    fprintf(stderr, "reading string from %llx\n", (long long)address);
    char c;
    do {
        plcrash_error_t err = plcrash_async_read_addr(image->task, cursor, &c, 1);
        fprintf(stderr, "Got error %d and char %x at position %llu\n", err, c, (long long)(cursor - address));
        if (err != PLCRASH_ESUCCESS)
            return err;
        cursor++;
    } while(c != 0);
    
    pl_vm_size_t length = cursor - address - 1;
    return plcrash_async_mobject_init(outMobj, image->task, address, length);
}

plcrash_error_t pl_async_objc_parse_from_module_info (pl_async_macho_t *image) {
    plcrash_error_t err = PLCRASH_EUNKNOWN;
    
    bool moduleMobjInitialized = false;
    plcrash_async_mobject_t moduleMobj;
    err = pl_async_macho_map_section(image, kObjCSegmentName, kObjCModuleInfoSectionName, &moduleMobj);
    if (err != PLCRASH_ESUCCESS)
        goto cleanup;
    
    moduleMobjInitialized = true;
    
    struct pl_objc_module *moduleData = plcrash_async_mobject_pointer(&moduleMobj, moduleMobj.address, sizeof(*moduleData));
    if (moduleData == NULL) {
        err = PLCRASH_ENOTFOUND;
        goto cleanup;
    }
    
    fprintf(stderr, "version=%d size=%d name=%x symtab=%x\n", image->swap32(moduleData->version), image->swap32(moduleData->size), image->swap32(moduleData->name), image->swap32(moduleData->symtab));
    
cleanup:
    if (moduleMobjInitialized)
        plcrash_async_mobject_free(&moduleMobj);
    
    return err;
}

plcrash_error_t pl_async_objc_parse_from_data_section (pl_async_macho_t *image) {
    plcrash_error_t err = PLCRASH_EUNKNOWN;
    
    fprintf(stderr, "parsing from data section\n");
    
    bool classMobjInitialized = false;
    plcrash_async_mobject_t classMobj;
    err = pl_async_macho_map_section(image, kDataSegmentName, kClassListSectionName, &classMobj);
    if (err != PLCRASH_ESUCCESS)
        goto cleanup;
    
    classMobjInitialized = true;
    
    fprintf(stderr, "reading classes\n");
    uint64_t *classPtrs = plcrash_async_mobject_pointer(&classMobj, classMobj.address, classMobj.length);
    fprintf(stderr, "reading from pointer %p, length is %d\n", classPtrs, (int)classMobj.length);
    for(unsigned i = 0; i < classMobj.length / sizeof(*classPtrs); i++) {
        pl_vm_address_t ptr = image->swap64(classPtrs[i]);
        
        fprintf(stderr, "%s:%d\n", __func__, __LINE__);
        fprintf(stderr, "reading class from %llx\n", (long long)ptr);
        struct pl_objc2_class class;
        err = plcrash_async_read_addr(image->task, ptr, &class, sizeof(class));
        if (err)
            goto cleanup;
        
        pl_vm_address_t dataPtr = image->swap64(class.data_rw) & ~3LL;
        fprintf(stderr, "%s:%d\n", __func__, __LINE__);
        fprintf(stderr, "reading RW class data from %llx\n", (long long)dataPtr);
        struct pl_objc2_class_data_rw_t classDataRW;
        err = plcrash_async_read_addr(image->task, dataPtr, &classDataRW, sizeof(classDataRW));
        if (err)
            goto cleanup;
        
        fprintf(stderr, "%s:%d\n", __func__, __LINE__);
        fprintf(stderr, "reading RO class data from %llx\n", image->swap64(classDataRW.data_ro));
        struct pl_objc2_class_data_ro_t classDataRO;
        err = plcrash_async_read_addr(image->task, image->swap64(classDataRW.data_ro), &classDataRO, sizeof(classDataRO));
        if (err)
            goto cleanup;
        
        fprintf(stderr, "%s:%d\n", __func__, __LINE__);
        fprintf(stderr, "Reading name from %llx\n", image->swap64(classDataRO.name));
        plcrash_async_mobject_t nameMobj;
        err = read_string(image, image->swap64(classDataRO.name), &nameMobj);
        if (err)
            goto cleanup;
        
        fprintf(stderr, "%s:%d\n", __func__, __LINE__);
        const char *name = plcrash_async_mobject_pointer(&nameMobj, nameMobj.address, nameMobj.length);
        if (name == NULL)
            name = "ERROR GETTING MOBJECT POINTER";
        fprintf(stderr, "Read class named %.*s\n", (int)nameMobj.length, name);
        plcrash_async_mobject_free(&nameMobj);
    }
    
cleanup:
    if (classMobjInitialized)
        plcrash_async_mobject_free(&classMobj);
    
    return err;
}

plcrash_error_t pl_async_objc_parse (pl_async_macho_t *image) {
    plcrash_error_t err;
    
    err = pl_async_objc_parse_from_module_info(image);
    if (err == PLCRASH_ENOTFOUND)
        err = pl_async_objc_parse_from_data_section(image);
    
    return err;
}

