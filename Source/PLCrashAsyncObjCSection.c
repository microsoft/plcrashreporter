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
#include <mach/mach_time.h>


static char * const kObjCSegmentName = "__OBJC";
static char * const kDataSegmentName = "__DATA";

static char * const kObjCModuleInfoSectionName = "__module_info";
static char * const kClassListSectionName = "__objc_classlist";
static char * const kObjCConstSectionName = "__objc_const";
static char * const kObjCDataSectionName = "__objc_data";

static uint32_t CLS_NO_METHOD_ARRAY = 0x4000;
static uint32_t END_OF_METHODS_LIST = -1;
static uint32_t RW_REALIZED = (1<<31);

struct pl_objc1_module {
    uint32_t version;
    uint32_t size;
    uint32_t name;
    uint32_t symtab;
};

struct pl_objc1_symtab {
    uint32_t sel_ref_cnt;
    uint32_t refs;
    uint16_t cls_def_count;
    uint16_t cat_def_count;
};

struct pl_objc1_class {
    uint32_t isa;
    uint32_t super;
    uint32_t name;
    uint32_t version;
    uint32_t info;
    uint32_t instance_size;
    uint32_t ivars;
    uint32_t methods;
    uint32_t cache;
    uint32_t protocols;
};

struct pl_objc1_method_list {
    uint32_t obsolete;
    uint32_t count;
};

struct pl_objc1_method {
    uint32_t name;
    uint32_t types;
    uint32_t imp;
};

struct pl_objc2_class_32 {
    uint32_t isa;
    uint32_t superclass;
    uint32_t cache;
    uint32_t vtable;
    uint32_t data_rw;
};

struct pl_objc2_class_64 {
    uint64_t isa;
    uint64_t superclass;
    uint64_t cache;
    uint64_t vtable;
    uint64_t data_rw;
};

struct pl_objc2_class_data_rw_32 {
    uint32_t flags;
    uint32_t version;
    uint32_t data_ro;
};

struct pl_objc2_class_data_rw_64 {
    uint32_t flags;
    uint32_t version;
    uint64_t data_ro;
};

struct pl_objc2_class_data_ro_32 {
    uint32_t flags;
    uint32_t instanceStart;
    uint32_t instanceSize;
    uint32_t ivarLayout;
    uint32_t name;
    uint32_t baseMethods;
    uint32_t baseProtocols;
    uint32_t ivars;
    uint32_t weakIvarLayout;
    uint32_t baseProperties;
};

struct pl_objc2_class_data_ro_64 {
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

struct pl_objc2_method_32 {
    uint32_t name;
    uint32_t types;
    uint32_t imp;
};

struct pl_objc2_method_64 {
    uint64_t name;
    uint64_t types;
    uint64_t imp;
};

struct pl_objc2_list_header {
    uint32_t entsize;
    uint32_t count;
};


static plcrash_error_t pl_async_parse_obj1_class(pl_async_macho_t *image, struct pl_objc1_class *class, bool isMetaClass, pl_async_objc_found_method_cb callback, void *ctx) {
    plcrash_error_t err;
    
    /* Get the class's name. */
    pl_vm_address_t namePtr = image->swap32(class->name);
    bool classNameInitialized = false;
    plcrash_async_macho_string_t className;
    err = plcrash_async_macho_string_init(&className, image, namePtr);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("plcrash_async_macho_string_init at 0x%llx error %d", (long long)namePtr, err);
        goto cleanup;
    }
    classNameInitialized = true;
    
    /* Grab the method list pointer. This is either a pointer to
     * a single method_list structure, OR a pointer to an array
     * of pointers to method_list structures, depending on the
     * flag in the .info field. Argh. */
    pl_vm_address_t methodListPtr = image->swap32(class->methods);
    
    /* If CLS_NO_METHOD_ARRAY is set, then methodListPtr points to
     * one method_list. If it's not set, then it points to an
     * array of pointers to method lists. */
    bool hasMultipleMethodLists = (image->swap32(class->info) & CLS_NO_METHOD_ARRAY) == 0;
    pl_vm_address_t methodListCursor = methodListPtr;
    
    while (true) {
        /* Grab a method list pointer. How to do that depends on whether
         * CLS_NO_METHOD_ARRAY is set. Once done, thisListPtr contains
         * a pointer to the method_list structure to read. */
        pl_vm_address_t thisListPtr;
        if (hasMultipleMethodLists) {
            /* If there are multiple method lists, then read the list pointer
             * from the current cursor, and advance the cursor. */
            uint32_t ptr;
            err = plcrash_async_read_addr(image->task, methodListCursor, &ptr, sizeof(ptr));
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)methodListCursor, err);
                goto cleanup;
            }
            
            thisListPtr = image->swap32(ptr);
            /* The end of the list is indicated with NULL or
             * END_OF_METHODS_LIST (the ObjC runtime source checks both). */
            if (thisListPtr == 0 || thisListPtr == END_OF_METHODS_LIST)
                break;
            
            methodListCursor += sizeof(ptr);
        } else {
            /* If CLS_NO_METHOD_ARRAY is set, then the single method_list
             * is pointed to by the cursor. */
            thisListPtr = methodListCursor;
            
            /* The pointer may be NULL, in which case there are no methods. */
            if (thisListPtr == 0)
                break;
        }
        
        /* Read a method_list structure from the current list pointer. */
        struct pl_objc1_method_list methodList;
        err = plcrash_async_read_addr(image->task, thisListPtr, &methodList, sizeof(methodList));
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)methodListPtr, err);
            goto cleanup;
        }
        
        /* Find out how many methods are in the list, and iterate. */
        uint32_t count = image->swap32(methodList.count);
        for (uint32_t i = 0; i < count; i++) {
            /* Method structures are laid out directly following the
             * method_list structure. */
            struct pl_objc1_method method;
            pl_vm_address_t methodPtr = thisListPtr + sizeof(methodList) + i * sizeof(method);
            err = plcrash_async_read_addr(image->task, methodPtr, &method, sizeof(method));
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)methodPtr, err);
                goto cleanup;
            }
            
            /* Load the method name from the .name field pointer. */
            pl_vm_address_t methodNamePtr = image->swap32(method.name);
            plcrash_async_macho_string_t methodName;
            err = plcrash_async_macho_string_init(&methodName, image, methodNamePtr);
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("plcrash_async_macho_string_init at 0x%llx error %d", (long long)methodNamePtr, err);
                goto cleanup;
            }
            
            /* Grab the method's IMP as well. */
            pl_vm_address_t imp = image->swap32(method.imp);
            
            /* Callback! */
            callback(isMetaClass, &className, &methodName, imp, ctx);
            
            /* Clean up the method name object. */
            plcrash_async_macho_string_free(&methodName);
        }
        
        /* Bail out of the loop after a single iteration if
         * CLS_NO_METHOD_ARRAY is set, because there's no need
         * to iterate in that case. */
        if (!hasMultipleMethodLists)
            break;
    }
    
cleanup:
    if (classNameInitialized)
        plcrash_async_macho_string_free(&className);
    
    return err;
}

/**
 * Parse Objective-C class data from an old-style __module_info section containing
 * ObjC1 metadata.
 *
 * @param image The Mach-O image to read from.
 * @param callback The callback to invoke for each method found.
 * @param ctx The context pointer to pass to the callback.
 * @return PLCRASH_ESUCCESS on success, PLCRASH_ENOTFOUND if the image doesn't
 * contain ObjC1 metadata, or another error code if a different error occurred.
 */
static plcrash_error_t pl_async_objc_parse_from_module_info (pl_async_macho_t *image, pl_async_objc_found_method_cb callback, void *ctx) {
    plcrash_error_t err = PLCRASH_EUNKNOWN;
    
    /* Map the __module_info section. */
    bool moduleMobjInitialized = false;
    plcrash_async_mobject_t moduleMobj;
    err = pl_async_macho_map_section(image, kObjCSegmentName, kObjCModuleInfoSectionName, &moduleMobj);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("pl_async_macho_map_section(%p, %s, %s, %p) failure %d", image, kObjCSegmentName, kObjCModuleInfoSectionName, &moduleMobj, err);
        goto cleanup;
    }
    
    /* Successful mapping, so mark the memory object as needing cleanup. */
    moduleMobjInitialized = true;
    
    /* Get a pointer to the module info data. */
    struct pl_objc1_module *moduleData = plcrash_async_mobject_pointer(&moduleMobj, moduleMobj.address, sizeof(*moduleData));
    if (moduleData == NULL) {
        PLCF_DEBUG("Failed to obtain pointer from %s memory object", kObjCModuleInfoSectionName);
        err = PLCRASH_ENOTFOUND;
        goto cleanup;
    }
    
    /* Read successive module structs from the section until we run out of data. */
    for (unsigned moduleIndex = 0; moduleIndex < moduleMobj.length / sizeof(*moduleData); moduleIndex++) {
        /* Grab the pointer to the symtab for this module struct. */
        pl_vm_address_t symtabPtr = image->swap32(moduleData[moduleIndex].symtab);
        if (symtabPtr == 0)
            continue;
        
        /* Read a symtab struct from that pointer. */
        struct pl_objc1_symtab symtab;
        err = plcrash_async_read_addr(image->task, symtabPtr, &symtab, sizeof(symtab));
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)symtabPtr, err);
            goto cleanup;
        }
        
        /* Iterate over the classes in the symtab. */
        uint16_t classCount = image->swap16(symtab.cls_def_count);
        for (unsigned i = 0; i < classCount; i++) {
            /* Classes are indicated by pointers laid out sequentially after the
             * symtab structure. */
            uint32_t classPtr;
            pl_vm_address_t cursor = symtabPtr + sizeof(symtab) + i * sizeof(classPtr);
            err = plcrash_async_read_addr(image->task, cursor, &classPtr, sizeof(classPtr));
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)cursor, err);
                goto cleanup;
            }
            classPtr = image->swap32(classPtr);
            
            /* Read a class structure from the class pointer. */
            struct pl_objc1_class class;
            err = plcrash_async_read_addr(image->task, classPtr, &class, sizeof(class));
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)classPtr, err);
                goto cleanup;
            }
            
            err = pl_async_parse_obj1_class(image, &class, false, callback, ctx);
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("pl_async_parse_obj1_class error %d while parsing class", err);
                goto cleanup;
            }
            
            /* Read a class structure for the metaclass. */
            pl_vm_address_t isa = image->swap32(class.isa);
            struct pl_objc1_class metaclass;
            err = plcrash_async_read_addr(image->task, isa, &metaclass, sizeof(metaclass));
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)isa, err);
                goto cleanup;
            }
            
            err = pl_async_parse_obj1_class(image, &metaclass, true, callback, ctx);
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("pl_async_parse_obj1_class error %d while parsing metaclass", err);
                goto cleanup;
            }
        }
    }
    
cleanup:
    /* Clean up the memory objects before returning if they're initialized. */
    if (moduleMobjInitialized)
        plcrash_async_mobject_free(&moduleMobj);
    
    return err;
}

/**
 * Parse a single class from ObjC2 class data.
 *
 * @param image The image to read from.
 * @param class_32 A pointer to a 32-bit class structure. Only needs to be
 * filled out if the image is 32 bits.
 * @param class_64 A pointer to a 64-bit class structure. Only needs to be
 * filled out if the image is 64 bits.
 * @param callback The callback to invoke for each method found.
 * @param ctx A context pointer to pass to the callback.
 * @return An error code.
 */
static plcrash_error_t pl_async_objc_parse_objc2_class(pl_async_macho_t *image, plcrash_async_mobject_t *objCConstMobj, struct pl_objc2_class_32 *class_32, struct pl_objc2_class_64 *class_64, bool isMetaClass, pl_async_objc_found_method_cb callback, void *ctx) {
    plcrash_error_t err;
    
    /* Set up the class name string and a flag to determine whether it needs cleanup. */
    plcrash_async_macho_string_t className;
    bool classNameInitialized = false;
    
    /* Grab the class's data_rw pointer. This needs masking because it also
     * can contain flags. */
    pl_vm_address_t dataPtr = (image->m64
                               ? image->swap64(class_64->data_rw)
                               : image->swap32(class_32->data_rw));
    dataPtr &= ~(pl_vm_address_t)3;
    
    /* Read an architecture-appropriate class_rw structure for the class. */
    struct pl_objc2_class_data_rw_32 classDataRW_32;
    struct pl_objc2_class_data_rw_64 classDataRW_64;
    if (image->m64)
        err = plcrash_async_read_addr(image->task, dataPtr, &classDataRW_64, sizeof(classDataRW_64));
    else
        err = plcrash_async_read_addr(image->task, dataPtr, &classDataRW_32, sizeof(classDataRW_32));
    
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)dataPtr, err);
        goto cleanup;
    }
    
    /* Check the flags. If it's not yet realized, then we need to skip the class. */
    uint32_t flags;
    if (image->m64)
        flags = classDataRW_64.flags;
    else
        flags = classDataRW_32.flags;
    
    if ((flags & RW_REALIZED) == 0)  {
        PLCF_DEBUG("Found unrealized class with RO data at 0x%llx, skipping it", (long long)dataPtr);
        goto cleanup;
    }
    
    /* Grab the data_ro pointer. The RO data (read-only) contains the class name
     * and method list. */
    pl_vm_address_t dataROPtr = (image->m64
                                 ? image->swap64(classDataRW_64.data_ro)
                                 : image->swap32(classDataRW_32.data_ro));
    
    /* Grab the pointer to the class_ro structure. */
    struct pl_objc2_class_data_ro_32 *classDataRO_32;
    struct pl_objc2_class_data_ro_64 *classDataRO_64;
    pl_vm_size_t length = (image->m64 ? sizeof(*classDataRO_64) : sizeof(*classDataRO_32));
    void *classDataROPtr = plcrash_async_mobject_pointer(objCConstMobj, plcrash_async_mobject_remap_address(objCConstMobj, dataROPtr), length);
    
    if (classDataROPtr == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_pointer at 0x%llx returned NULL", (long long)dataROPtr);
        goto cleanup;
    }
    
    classDataRO_32 = classDataROPtr;
    classDataRO_64 = classDataROPtr;
    
    /* Fetch the pointer to the class name, and make the string. */
    pl_vm_address_t classNamePtr = (image->m64
                                    ? image->swap64(classDataRO_64->name)
                                    : image->swap32(classDataRO_32->name));
    err = plcrash_async_macho_string_init(&className, image, classNamePtr);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("plcrash_async_macho_string_init at 0x%llx error %d", (long long)classNamePtr, err);
        goto cleanup;
    }
    classNameInitialized = true;
    
    /* Fetch the pointer to the method list. */
    pl_vm_address_t methodsPtr = (image->m64
                                  ? image->swap64(classDataRO_64->baseMethods)
                                  : image->swap32(classDataRO_32->baseMethods));
    if (methodsPtr == 0)
        goto cleanup;
    
    /* Read the method list header. */
    struct pl_objc2_list_header *header;
    header = plcrash_async_mobject_pointer(objCConstMobj, plcrash_async_mobject_remap_address(objCConstMobj, methodsPtr), sizeof(header));
    if (header == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_pointer in objCConstMobj failed to map methods pointer 0x%llx", (long long)methodsPtr);
        goto cleanup;
    }
    
    /* Extract the entry size and count from the list header. */
    uint32_t entsize = image->swap32(header->entsize) & ~(uint32_t)3;
    uint32_t count = image->swap32(header->count);
    
    /* Compute the method list start position and length. */
    pl_vm_address_t methodListStart = methodsPtr + sizeof(*header);
    pl_vm_size_t methodListLength = (pl_vm_size_t)entsize * count;
    
    const char *cursor = plcrash_async_mobject_pointer(objCConstMobj, plcrash_async_mobject_remap_address(objCConstMobj, methodListStart), methodListLength);
    if (cursor == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_pointer at 0x%llx length %llu returned NULL", (long long)methodListStart, (unsigned long long)methodListLength);
        goto cleanup;
    }
    
    /* Extract methods from the list. */
    for (uint32_t i = 0; i < count; i++) {
        /* Read an architecture-appropriate method structure from the
         * current cursor. */
        const struct pl_objc2_method_32 *method_32 = (void *)cursor;
        const struct pl_objc2_method_64 *method_64 = (void *)cursor;
        
        /* Extract the method name pointer. */
        pl_vm_address_t methodNamePtr = (image->m64
                                         ? image->swap64(method_64->name)
                                         : image->swap32(method_32->name));
        
        /* Read the method name. */
        plcrash_async_macho_string_t methodName;
        err = plcrash_async_macho_string_init(&methodName, image, methodNamePtr);
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("plcrash_async_macho_string_init at 0x%llx error %d", (long long)methodNamePtr, err);
            goto cleanup;
        }
        
        /* Extract the method IMP. */
        pl_vm_address_t imp = (image->m64
                               ? image->swap64(method_64->imp)
                               : image->swap32(method_32->imp));
        
        /* Call the callback. */
        callback(isMetaClass, &className, &methodName, imp, ctx);
        
        /* Clean up the method name. */
        plcrash_async_macho_string_free(&methodName);
        
        /* Increment the cursor by the entry size for the next iteration of the loop. */
        cursor += entsize;
    }
    
cleanup:
    if (classNameInitialized)
        plcrash_async_macho_string_free(&className);
    
    return err;
}

/**
 * Parse ObjC2 class data from a __objc_classlist section.
 *
 * @param image The Mach-O image to parse.
 * @param callback The callback to invoke for each method found.
 * @param ctx A context pointer to pass to the callback.
 * @return PLCRASH_ESUCCESS on success, PLCRASH_ENOTFOUND if no ObjC2 data
 * exists in the image, and another error code if a different error occurred.
 */
static plcrash_error_t pl_async_objc_parse_from_data_section (pl_async_macho_t *image, pl_async_objc_found_method_cb callback, void *ctx) {
    plcrash_error_t err = PLCRASH_EUNKNOWN;
    
    /* Set up memory objects, and flags so the cleanup code knows what to free. */
    bool objcConstMobjInitialized = false;
    plcrash_async_mobject_t objcConstMobj;
    bool classMobjInitialized = false;
    plcrash_async_mobject_t classMobj;
    bool objcDataMobjInitialized = false;
    plcrash_async_mobject_t objcDataMobj;

    /* Map in the __objc_const section, which is where all the read-only class data lives. */
    err = pl_async_macho_map_section(image, kDataSegmentName, kObjCConstSectionName, &objcConstMobj);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("pl_async_macho_map_section(%p, %s, %s, %p) failure %d", image, kDataSegmentName, kObjCConstSectionName, &objcConstMobj, err);
        goto cleanup;
    }
    objcConstMobjInitialized = true;
    
    /* Map in the class list section.  */
    err = pl_async_macho_map_section(image, kDataSegmentName, kClassListSectionName, &classMobj);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("pl_async_macho_map_section(%p, %s, %s, %p) failure %d", image, kDataSegmentName, kClassListSectionName, &classMobj, err);
        goto cleanup;
    }
    classMobjInitialized = true;
    
    /* Map in the __objc_data section, which is where the actual classes live. */
    err = pl_async_macho_map_section(image, kDataSegmentName, kObjCDataSectionName, &objcDataMobj);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("pl_async_macho_map_section(%p, %s, %s, %p) failure %d", image, kDataSegmentName, kObjCDataSectionName, &objcDataMobj, err);
        goto cleanup;
    }
    objcDataMobjInitialized = true;
    
    /* Get a pointer out of the mapped class list. */
    void *classPtrs = plcrash_async_mobject_pointer(&classMobj, classMobj.address, classMobj.length);
    if (classPtrs == NULL) {
        PLCF_DEBUG("plcrash_async_mobject_pointer in objcConstMobj for pointer %llx returned NULL", (long long)classMobj.address);
        goto cleanup;
    }
    
    /* Class pointers are 32 or 64 bits depending on architectures. Set up one
     * pointer for each. */
    uint32_t *classPtrs_32 = classPtrs;
    uint64_t *classPtrs_64 = classPtrs;
    
    /* Figure out how many classes are in the class list based on its length and
     * the size of a pointer in the image. */
    unsigned classCount = classMobj.length / (image->m64 ? sizeof(*classPtrs_64) : sizeof(*classPtrs_32));
    
    /* Iterate over all classes. */
    for(unsigned i = 0; i < classCount; i++) {
        /* Read a class pointer at the current index from the appropriate pointer. */
        pl_vm_address_t ptr = (image->m64
                               ? image->swap64(classPtrs_64[i])
                               : image->swap32(classPtrs_32[i]));
        
        /* Read an architecture-appropriate class structure. */
        struct pl_objc2_class_32 *class_32;
        struct pl_objc2_class_64 *class_64;
        void *classPtr = plcrash_async_mobject_pointer(&objcDataMobj, plcrash_async_mobject_remap_address(&objcDataMobj, ptr), image->m64 ? sizeof(*class_64) : sizeof(*class_32));
        if (classPtr == NULL) {
            PLCF_DEBUG("plcrash_async_mobject_pointer in objcDataMobj for pointer %llx returned NULL", (long long)ptr);
            goto cleanup;
        }
        
        class_32 = classPtr;
        class_64 = classPtr;
        
        /* Parse the class. */
        err = pl_async_objc_parse_objc2_class(image, &objcConstMobj, class_32, class_64, false, callback, ctx);
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("pl_async_objc_parse_objc2_class error %d while parsing class", err);
            goto cleanup;
        }
        
        /* Read an architecture-appropriate class structure for the metaclass. */
        pl_vm_address_t isa = (image->m64
                               ? image->swap64(class_64->isa)
                               : image->swap32(class_32->isa));
        struct pl_objc2_class_32 *metaclass_32;
        struct pl_objc2_class_64 *metaclass_64;
        void *metaclassPtr = plcrash_async_mobject_pointer(&objcDataMobj, plcrash_async_mobject_remap_address(&objcDataMobj, isa), image->m64 ? sizeof(*class_64) : sizeof(*class_32));
        if (metaclassPtr == NULL) {
            PLCF_DEBUG("plcrash_async_mobject_pointer in objcDataMobj for pointer %llx returned NULL", (long long)isa);
            goto cleanup;
        }
        
        metaclass_32 = metaclassPtr;
        metaclass_64 = metaclassPtr;
        
        /* Parse the metaclass. */
        err = pl_async_objc_parse_objc2_class(image, &objcConstMobj, metaclass_32, metaclass_64, true, callback, ctx);
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("pl_async_objc_parse_objc2_class error %d while parsing metaclass", err);
            goto cleanup;
        }
    }
    
cleanup:
    /* Clean up the memory objects if they're currently initialized. */
    if (objcConstMobjInitialized)
        plcrash_async_mobject_free(&objcConstMobj);
    if (classMobjInitialized)
        plcrash_async_mobject_free(&classMobj);
    if (objcDataMobjInitialized)
        plcrash_async_mobject_free(&objcDataMobj);
    
    return err;
}

/**
 * Parse Objective-C class data from a Mach-O image, invoking a callback
 * for each method found in the data. This tries both old-style ObjC1
 * class data and new-style ObjC2 data.
 *
 * @param image The image to read class data from.
 * @param callback The callback to invoke for each method.
 * @param ctx The context pointer to pass to the callback.
 * @return An error code.
 */
plcrash_error_t pl_async_objc_parse (pl_async_macho_t *image, pl_async_objc_found_method_cb callback, void *ctx) {
    plcrash_error_t err;
   
    /* Try ObjC1 data. */
    err = pl_async_objc_parse_from_module_info(image, callback, ctx);
    
    /* If there wasn't any, try ObjC2 data. */
    if (err == PLCRASH_ENOTFOUND)
        err = pl_async_objc_parse_from_data_section(image, callback, ctx);
    
    return err;
}

struct pl_async_objc_find_method_search_context {
    pl_vm_address_t searchIMP;
    pl_vm_address_t bestIMP;
};

struct pl_async_objc_find_method_call_context {
    pl_vm_address_t searchIMP;
    pl_async_objc_found_method_cb outerCallback;
    void *outerCallbackCtx;
};

/**
 * Callback used to search for the method IMP that best matches a search target.
 * The context pointer is a pointer to pl_async_objc_find_method_search_context.
 * The searchIMP field should be set to the IMP to search for. The bestIMP field
 * should be initialized to 0, and will be updated with the best-matching IMP
 * found.
 */
static void pl_async_objc_find_method_search_callback (bool isClassMethod, plcrash_async_macho_string_t *className, plcrash_async_macho_string_t *methodName, pl_vm_address_t imp, void *ctx) {
    struct pl_async_objc_find_method_search_context *ctxStruct = ctx;
    
    if (imp >= ctxStruct->bestIMP && imp <= ctxStruct->searchIMP) {
        ctxStruct->bestIMP = imp;
    }
}

/**
 * Callback used to find the method that precisely matches a search target.
 * The context pointer is a pointer to pl_async_objc_find_method_call_context.
 * The searchIMP field should be set to the IMP to search for. The outerCallback
 * will be invoked, passing outerCalblackCtx and the method data for a precise
 * match, if any is found.
 */
static void pl_async_objc_find_method_call_callback (bool isClassMethod, plcrash_async_macho_string_t *className, plcrash_async_macho_string_t *methodName, pl_vm_address_t imp, void *ctx) {
    struct pl_async_objc_find_method_call_context *ctxStruct = ctx;
    
    if (imp == ctxStruct->searchIMP && ctxStruct->outerCallback != NULL) {
        ctxStruct->outerCallback(isClassMethod, className, methodName, imp, ctxStruct->outerCallbackCtx);
        ctxStruct->outerCallback = NULL;
    }
}

/**
 * Search for the method that best matches the given code address.
 *
 * @param image The image to search.
 * @param imp The address to search for.
 * @param callback The callback to invoke when the best match is found.
 * @param ctx The context pointer to pass to the callback.
 * @return An error code.
 */
plcrash_error_t pl_async_objc_find_method (pl_async_macho_t *image, pl_vm_address_t imp, pl_async_objc_found_method_cb callback, void *ctx) {
    struct pl_async_objc_find_method_search_context searchCtx = {
        .searchIMP = imp
    };
    
    plcrash_error_t err = pl_async_objc_parse(image, pl_async_objc_find_method_search_callback, &searchCtx);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("pl_async_objc_parse(%p, 0x%llx, %p, %p) failure %d", image, (long long)imp, callback, ctx, err);
        return err;
    }
    
    if (searchCtx.bestIMP == 0)
        return PLCRASH_ENOTFOUND;
    
    struct pl_async_objc_find_method_call_context callCtx = {
        .searchIMP = searchCtx.bestIMP,
        .outerCallback = callback,
        .outerCallbackCtx = ctx
    };
    
    return pl_async_objc_parse(image, pl_async_objc_find_method_call_callback, &callCtx);
}

