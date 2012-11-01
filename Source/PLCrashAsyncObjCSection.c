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

static uint32_t CLS_NO_METHOD_ARRAY = 0x4000;
static uint32_t END_OF_METHODS_LIST = -1;

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


/**
 * Read a NUL-terminated C string from a Mach-O image at a given address.
 *
 * @param image The Mach-O image to read.
 * @param address The address of the start of the string.
 * @param outMobj On successful return, contains a memory object representing the
 * string. This memory does NOT include the terminating NUL.
 * @return An error code.
 */
static plcrash_error_t read_string (pl_async_macho_t *image, pl_vm_address_t address, plcrash_async_mobject_t *outMobj) {
    pl_vm_address_t cursor = address;
    
    char c;
    do {
        /* Read successive characters until we reach a 0. */
        plcrash_error_t err = plcrash_async_read_addr(image->task, cursor, &c, 1);
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("in read_string(%p, 0x%llx, %p), plcrash_async_read_addr at 0x%llx failure %d ", image, (long long)address, outMobj, (long long)cursor, err);
            return err;
        }
        cursor++;
    } while(c != 0);
    
    /* Compute the length of the string data and make a new memory object. */
    pl_vm_size_t length = cursor - address - 1;
    return plcrash_async_mobject_init(outMobj, image->task, address, length);
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
plcrash_error_t pl_async_objc_parse_from_module_info (pl_async_macho_t *image, pl_async_objc_found_method_cb callback, void *ctx) {
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
    
    /* Set up a memory object for the class name, and allow it to be cleaned up
     * at the end if necessary. */
    bool classNameMobjInitialized = false;
    plcrash_async_mobject_t classNameMobj;
    
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
            
            /* If the class name memory object is already in use, destroy it so it
             * can be reused. */
            if (classNameMobjInitialized) {
                plcrash_async_mobject_free(&classNameMobj);
                classNameMobjInitialized = false;
            }
            
            /* Read the class's name. */
            pl_vm_address_t namePtr = image->swap32(class.name);
            err = read_string(image, namePtr, &classNameMobj);
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("read_string at 0x%llx error %d", (long long)namePtr, err);
                goto cleanup;
            }
            classNameMobjInitialized = true;
            
            /* Get a pointer out of the class name memory object. */
            const char *className = plcrash_async_mobject_pointer(&classNameMobj, classNameMobj.address, classNameMobj.length);
            if (className == NULL) {
                PLCF_DEBUG("Failed to get pointer to class name data");
                goto cleanup;
            }
            
            /* Grab the method list pointer. This is either a pointer to
             * a single method_list structure, OR a pointer to an array
             * of pointers to method_list structures, depending on the
             * flag in the .info field. Argh. */
            pl_vm_address_t methodListPtr = image->swap32(class.methods);
            
            /* If CLS_NO_METHOD_ARRAY is set, then methodListPtr points to
             * one method_list. If it's not set, then it points to an
             * array of pointers to method lists. */
            bool hasMultipleMethodLists = (image->swap32(class.info) & CLS_NO_METHOD_ARRAY) == 0;
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
                    plcrash_async_mobject_t methodNameMobj;
                    err = read_string(image, methodNamePtr, &methodNameMobj);
                    if (err != PLCRASH_ESUCCESS) {
                        PLCF_DEBUG("read_string at 0x%llx error %d", (long long)methodNamePtr, err);
                        goto cleanup;
                    }
                    
                    /* Grab a pointer to the method name. */
                    const char *methodName = plcrash_async_mobject_pointer(&methodNameMobj, methodNameMobj.address, methodNameMobj.length);
                    if (methodName == NULL) {
                        PLCF_DEBUG("Failed to get method name pointer");
                        plcrash_async_mobject_free(&methodNameMobj);
                        goto cleanup;
                    }
                    
                    /* Grab the method's IMP as well. */
                    pl_vm_address_t imp = image->swap32(method.imp);
                    
                    /* Callback! */
                    callback(className, classNameMobj.length, methodName, methodNameMobj.length, imp, ctx);
                    
                    /* Clean up the method name object. */
                    plcrash_async_mobject_free(&methodNameMobj);
                }
                
                /* Bail out of the loop after a single iteration if
                 * CLS_NO_METHOD_ARRAY is set, because there's no need
                 * to iterate in that case. */
                if (!hasMultipleMethodLists)
                    break;
            }
        }
    }
    
cleanup:
    /* Clean up the memory objects before returning if they're initialized. */
    if (moduleMobjInitialized)
        plcrash_async_mobject_free(&moduleMobj);
    if (classNameMobjInitialized)
        plcrash_async_mobject_free(&classNameMobj);
    
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
plcrash_error_t pl_async_objc_parse_from_data_section (pl_async_macho_t *image, pl_async_objc_found_method_cb callback, void *ctx) {
    plcrash_error_t err = PLCRASH_EUNKNOWN;
    
    /* Map in the class list section. Set a flag so the cleanup code knows whether
     * to free it afterwards. */
    bool classMobjInitialized = false;
    plcrash_async_mobject_t classMobj;
    err = pl_async_macho_map_section(image, kDataSegmentName, kClassListSectionName, &classMobj);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("pl_async_macho_map_section(%p, %s, %s, %p) failure %d", image, kDataSegmentName, kClassListSectionName, &classMobj, err);
        goto cleanup;
    }
    
    /* The class list was successfully mapped, so flag it for cleanup. */
    classMobjInitialized = true;
    
    /* Declare a memory object to hold class names, and a flag to determine whether
     * it needs cleanup at the end. */
    plcrash_async_mobject_t classNameMobj;
    bool classNameMobjInitialized = false;
    
    /* Get a pointer out of the mapped class list. */
    void *classPtrs = plcrash_async_mobject_pointer(&classMobj, classMobj.address, classMobj.length);
    
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
        struct pl_objc2_class_32 class_32;
        struct pl_objc2_class_64 class_64;
        if (image->m64)
            err = plcrash_async_read_addr(image->task, ptr, &class_64, sizeof(class_64));
        else
            err = plcrash_async_read_addr(image->task, ptr, &class_32, sizeof(class_32));
        
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)ptr, err);
            goto cleanup;
        }
        
        /* Grab the class's data_rw pointer. This needs masking because it also
         * can contain flags. */
        pl_vm_address_t dataPtr = (image->m64
                                   ? image->swap64(class_64.data_rw)
                                   : image->swap32(class_32.data_rw));
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
        
        /* Grab the data_ro pointer. The RO data (read-only) contains the class name
         * and method list. */
        pl_vm_address_t dataROPtr = (image->m64
                                     ? image->swap64(classDataRW_64.data_ro)
                                     : image->swap32(classDataRW_32.data_ro));
        
        /* Read an architecture-appropriate class_ro structure. */
        struct pl_objc2_class_data_ro_32 classDataRO_32;
        struct pl_objc2_class_data_ro_64 classDataRO_64;
        if (image->m64)
            err = plcrash_async_read_addr(image->task, dataROPtr, &classDataRO_64, sizeof(classDataRO_64));
        else
            err = plcrash_async_read_addr(image->task, dataROPtr, &classDataRO_32, sizeof(classDataRO_32));
        
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)dataROPtr, err);
            goto cleanup;
        }
        
        /* If the class name memory object is already in use, destroy it so it
         * can be reused. */
        if (classNameMobjInitialized) {
            plcrash_async_mobject_free(&classNameMobj);
            classNameMobjInitialized = false;
        }
        
        /* Fetch the pointer to the class name, and read the string. */
        pl_vm_address_t classNamePtr = (image->m64
                                        ? image->swap64(classDataRO_64.name)
                                        : image->swap32(classDataRO_32.name));
        err = read_string(image, classNamePtr, &classNameMobj);
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("read_string at 0x%llx error %d", (long long)classNamePtr, err);
            goto cleanup;
        }
        classNameMobjInitialized = true;
        
        /* Get a pointer to the class name string. */
        const char *className = plcrash_async_mobject_pointer(&classNameMobj, classNameMobj.address, classNameMobj.length);
        if (className == NULL) {
            PLCF_DEBUG("Failed to obtain pointer from class name memory object with address 0x%llx length %llu", (long long)classNameMobj.address, (unsigned long long)classNameMobj.length);
            err = PLCRASH_EACCESS;
            goto cleanup;
        }
        
        /* Fetch the pointer to the method list. */
        pl_vm_address_t methodsPtr = (image->m64
                                      ? image->swap64(classDataRO_64.baseMethods)
                                      : image->swap32(classDataRO_32.baseMethods));
        
        /* Read the method list header. */
        struct pl_objc2_list_header header;
        err = plcrash_async_read_addr(image->task, methodsPtr, &header, sizeof(header));
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)methodsPtr, err);
            goto cleanup;
        }
        
        /* Extract the entry size and count from the list header. */
        uint32_t entsize = image->swap32(header.entsize) & ~(uint32_t)3;
        uint32_t count = image->swap32(header.count);
        
        /* Start iterating right at the end of the header. */
        pl_vm_address_t cursor = methodsPtr + sizeof(header);
        
        /* Extract methods from the list. */
        for (uint32_t i = 0; i < count; i++) {
            /* Read an architecture-appropriate method structure from the
             * current cursor. */
            struct pl_objc2_method_32 method_32;
            struct pl_objc2_method_64 method_64;
            if (image->m64)
                err = plcrash_async_read_addr(image->task, cursor, &method_64, sizeof(method_64));
            else
                err = plcrash_async_read_addr(image->task, cursor, &method_32, sizeof(method_32));
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("plcrash_async_read_addr at 0x%llx error %d", (long long)cursor, err);
                goto cleanup;
            }
            
            /* Extract the method name pointer. */
            pl_vm_address_t methodNamePtr = (image->m64
                                             ? image->swap64(method_64.name)
                                             : image->swap32(method_32.name));
            
            /* Read the method name. */
            plcrash_async_mobject_t methodNameMobj;
            err = read_string(image, methodNamePtr, &methodNameMobj);
            if (err != PLCRASH_ESUCCESS) {
                PLCF_DEBUG("read_string at 0x%llx error %d", (long long)methodNamePtr, err);
                goto cleanup;
            }
            
            /* Get a pointer to the method name string. */
            const char *methodName = plcrash_async_mobject_pointer(&methodNameMobj, methodNameMobj.address, methodNameMobj.length);
            if (methodName == NULL) {
                PLCF_DEBUG("Failed to obtain pointer from method name memory object with address 0x%llx length %llu", (long long)methodNameMobj.address, (unsigned long long)methodNameMobj.length);
                plcrash_async_mobject_free(&methodNameMobj);
                err = PLCRASH_EACCESS;
                goto cleanup;
            }
            
            /* Extract the method IMP. */
            pl_vm_address_t imp = (image->m64
                                   ? image->swap64(method_64.imp)
                                   : image->swap32(method_32.imp));
            
            /* Call the callback. */
            callback(className, classNameMobj.length, methodName, methodNameMobj.length, imp, ctx);
            
            /* Clean up the method name memory object. */
            plcrash_async_mobject_free(&methodNameMobj);
            
            /* Increment the cursor by the entry size for the next iteration of the loop. */
            cursor += entsize;
        }
    }
    
cleanup:
    /* Clean up the memory objects if they're currently initialized. */
    if (classMobjInitialized)
        plcrash_async_mobject_free(&classMobj);
    if (classNameMobjInitialized)
        plcrash_async_mobject_free(&classNameMobj);
    
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
static void pl_async_objc_find_method_search_callback (const char *className, pl_vm_size_t classNameLength, const char *methodName, pl_vm_size_t methodNameLength, pl_vm_address_t imp, void *ctx) {
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
static void pl_async_objc_find_method_call_callback (const char *className, pl_vm_size_t classNameLength, const char *methodName, pl_vm_size_t methodNameLength, pl_vm_address_t imp, void *ctx) {
    struct pl_async_objc_find_method_call_context *ctxStruct = ctx;
    
    if (imp == ctxStruct->searchIMP) {
        ctxStruct->outerCallback(className, classNameLength, methodName, methodNameLength, imp, ctxStruct->outerCallbackCtx);
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

