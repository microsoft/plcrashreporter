/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 * Author: Gwynne Raskind <gwynne@darkrainfall.org>
 *
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

#include "PLCrashAsyncCompactUnwindEncoding.h"

#include <inttypes.h>

/**
 * @internal
 * @ingroup plcrash_async
 * @defgroup plcrash_async_cfe Compact Frame Encoding
 *
 * Implements async-safe parsing of compact frame unwind encodings.
 * @{
 */

/**
 * Initialize a new CFE reader using the provided memory object. Any resources held by a successfully initialized
 * instance must be freed via plcrash_async_cfe_reader_free();
 *
 * @param reader The reader instance to initialize.
 * @param mobj The memory object containing CFE data at the start address. This instance must survive for the lifetime
 * of the reader.
 * @param cpu_type The target architecture of the CFE data, encoded as a Mach-O CPU type. Interpreting CFE data is
 * architecture-specific, and Apple has not defined encodings for all supported architectures.
 */
plcrash_error_t plcrash_async_cfe_reader_init (plcrash_async_cfe_reader_t *reader, plcrash_async_mobject_t *mobj, cpu_type_t cputype) {
    reader->mobj = mobj;
    reader->cputype = cputype;

    /* Determine the expected encoding */
    switch (cputype) {
        case CPU_TYPE_X86:
        case CPU_TYPE_X86_64:
            reader->byteorder = plcrash_async_byteorder_little_endian();
            break;

        default:
            PLCF_DEBUG("Unsupported CPU type: %" PRIu32, cputype);
            return PLCRASH_ENOTSUP;
    }

    /* Fetch and verify the header */
    pl_vm_address_t base_addr = plcrash_async_mobject_base_address(mobj);
    struct unwind_info_section_header *header = plcrash_async_mobject_remap_address(mobj, base_addr, 0, sizeof(*header));
    if (header == NULL) {
        PLCF_DEBUG("Could not map the unwind info section header");
        return PLCRASH_EINVAL;
    }

    /* Verify the format version */
    uint32_t version = reader->byteorder->swap32(header->version);
    if (version != 1) {
        PLCF_DEBUG("Unsupported CFE version: %" PRIu32, version);
        return PLCRASH_ENOTSUP;
    }

    reader->header = *header;
    return PLCRASH_ESUCCESS;
}

/**
 * @internal
 *
 * Binary search in macro form. Pass the table, count, and result
 * pointers.
 *
 * CFE_FUN_BINARY_SEARCH_ENTVAL must also be defined, and it must
 * return the integer value to be compared.
 */
#define CFE_FUN_BINARY_SEARCH(_pc, _table, _count, _result) do { \
    uint32_t min = 0; \
    uint32_t mid = 0; \
    uint32_t max = _count - 1; \
\
    /* Search while _table[min:max] is not empty */ \
    while (max >= min) { \
        /* Calculate midpoint */ \
        mid = (min + max) / 2; \
\
        /* Determine which half of the array to search */ \
        uint32_t mid_fun_offset = CFE_FUN_BINARY_SEARCH_ENTVAL(_table[mid]); \
        if (mid_fun_offset < _pc) { \
            /* Check for inclusive equality */ \
            if (mid == max || CFE_FUN_BINARY_SEARCH_ENTVAL(_table[mid+1]) > _pc) { \
                _result = &_table[mid]; \
                break; \
            } \
\
\
            /* Base our search on the upper array */ \
            min = mid + 1; \
        } else if (mid_fun_offset > _pc) { \
            /* Base our search on the lower array */ \
            max = mid - 1; \
        } else if (mid_fun_offset == _pc) { \
            /* Direct match found */ \
            _result = &_table[mid]; \
            break; \
        } \
    } \
\
\
    /* The final entry must always match remaining PC values */ \
    PLCF_ASSERT(_result != NULL); \
} while (0);

/* Evaluates to true if the length of @a _ecount * @a sizof(_etype) can not be represented
 * by size_t. */
#define VERIFY_SIZE_T(_etype, _ecount) (SIZE_MAX / sizeof(_etype) < _ecount)

/**
 * TODO
 *
 * @param reader The initialized CFE reader.
 * @param pc The PC value to search for within the CFE data. Note that this value must be relative to
 * the target Mach-O image's __TEXT vmaddr.
 */
plcrash_error_t plcrash_async_cfe_reader_find_pc (plcrash_async_cfe_reader_t *reader, pl_vm_address_t pc) {
    const plcrash_async_byteorder_t *byteorder = reader->byteorder;
    const pl_vm_address_t base_addr = plcrash_async_mobject_base_address(reader->mobj);

    /* Find and map the common encodings table */
    uint32_t common_enc_count = byteorder->swap32(reader->header.commonEncodingsArrayCount);
    uint32_t *common_enc;
    {
        if (VERIFY_SIZE_T(uint32_t, common_enc_count)) {
            PLCF_DEBUG("CFE common encoding count extends beyond the range of size_t");
            return PLCRASH_EINVAL;
        }

        size_t common_enc_len = common_enc_count * sizeof(uint32_t);
        uint32_t common_enc_off = byteorder->swap32(reader->header.commonEncodingsArraySectionOffset);
        common_enc = plcrash_async_mobject_remap_address(reader->mobj, base_addr, common_enc_off, common_enc_len);
    }

    /* Find and load the first level entry */
    struct unwind_info_section_header_index_entry *first_level_entry = NULL;
    {
        /* Find and map the index */
        uint32_t index_off = byteorder->swap32(reader->header.indexSectionOffset);
        uint32_t index_count = byteorder->swap32(reader->header.indexCount);
        
        if (VERIFY_SIZE_T(sizeof(struct unwind_info_section_header_index_entry), index_count)) {
            PLCF_DEBUG("CFE index count extends beyond the range of size_t");
            return PLCRASH_EINVAL;
        }
        
        if (index_count == 0) {
            PLCF_DEBUG("CFE index contains no entries");
            return PLCRASH_ENOTFOUND;
        }
        
        /*
         * NOTE: CFE includes an extra entry in the total count of second-level pages, ie, from ld64:
         * const uint32_t indexCount = secondLevelPageCount+1;
         *
         * There's no explanation as to why, and tools appear to explicitly ignore the entry entirely. We do the same
         * here.
         */
        PLCF_ASSERT(index_count != 0);
        index_count--;
        
        /* Load the index entries */
        size_t index_len = index_count * sizeof(struct unwind_info_section_header_index_entry);
        struct unwind_info_section_header_index_entry *index_entries = plcrash_async_mobject_remap_address(reader->mobj, base_addr, index_off, index_len);
        if (index_entries == NULL) {
            PLCF_DEBUG("The declared entries table lies outside the mapped CFE range");
            return PLCRASH_EINVAL;
        }
        
        /* Binary search for the first-level entry */
#define CFE_FUN_BINARY_SEARCH_ENTVAL(_tval) (byteorder->swap32(_tval.functionOffset))
        CFE_FUN_BINARY_SEARCH(pc, index_entries, index_count, first_level_entry);
#undef CFE_FUN_BINARY_SEARCH_ENTVAL
    }

    /* Locate and decode the second-level entry */
    uint32_t second_level_offset = byteorder->swap32(first_level_entry->secondLevelPagesSectionOffset);
    uint32_t *second_level_kind = plcrash_async_mobject_remap_address(reader->mobj, base_addr, second_level_offset, sizeof(uint32_t));
    switch (byteorder->swap32(*second_level_kind)) {
        case UNWIND_SECOND_LEVEL_REGULAR: {
            struct unwind_info_regular_second_level_page_header *header;
            header = plcrash_async_mobject_remap_address(reader->mobj, base_addr, second_level_offset, sizeof(*header));
            if (header == NULL) {
                PLCF_DEBUG("The second-level page header lies outside the mapped CFE range");
                return PLCRASH_EINVAL;
            }

            PLCF_DEBUG("Regular %p!", header);
            
            // TODO - unimplemented!
            __builtin_trap();
            break;
        }

        case UNWIND_SECOND_LEVEL_COMPRESSED: {
            struct unwind_info_compressed_second_level_page_header *header;
            header = plcrash_async_mobject_remap_address(reader->mobj, base_addr, second_level_offset, sizeof(*header));
            if (header == NULL) {
                PLCF_DEBUG("The second-level page header lies outside the mapped CFE range");
                return PLCRASH_EINVAL;
            }
            
            /* Record the base offset */
            uint32_t base_foffset = byteorder->swap32(first_level_entry->functionOffset);

            /* Find the entries array */
            uint32_t entries_offset = byteorder->swap16(header->entryPageOffset);
            uint32_t entries_count = byteorder->swap16(header->entryCount);

            if (VERIFY_SIZE_T(sizeof(uint32_t), entries_count)) {
                PLCF_DEBUG("CFE second level entry count extends beyond the range of size_t");
                return PLCRASH_EINVAL;
            }
            
            if (!plcrash_async_mobject_verify_local_pointer(reader->mobj, header, entries_offset, entries_count * sizeof(uint32_t))) {
                PLCF_DEBUG("CFE entries table lies outside the mapped CFE range");
                return PLCRASH_EINVAL;
            }

            /* Binary search for the target entry */
            uint32_t *compressed_entries = (uint32_t *) (((uintptr_t)header) + entries_offset);
            uint32_t *c_entry_ptr = NULL;

#define CFE_FUN_BINARY_SEARCH_ENTVAL(_tval) (base_foffset + UNWIND_INFO_COMPRESSED_ENTRY_FUNC_OFFSET(byteorder->swap32(_tval)))
            CFE_FUN_BINARY_SEARCH(pc, compressed_entries, byteorder->swap16(header->entryCount), c_entry_ptr);
#undef CFE_FUN_BINARY_SEARCH_ENTVAL

            /* Find the actual encoding */
            uint32_t c_entry = byteorder->swap32(*c_entry_ptr);
            uint8_t c_encoding_idx = UNWIND_INFO_COMPRESSED_ENTRY_ENCODING_INDEX(c_entry);
            if (c_encoding_idx < common_enc_count) {
                /* Found in the common table */
                // TODO
                __builtin_trap();
            }
            
            
            PLCF_DEBUG("FOUND func_offset=%" PRIx32 " idx = %" PRIx32, base_foffset + UNWIND_INFO_COMPRESSED_ENTRY_FUNC_OFFSET(c_entry), UNWIND_INFO_COMPRESSED_ENTRY_ENCODING_INDEX(c_entry));
            
            break;
        }

        default:
            PLCF_DEBUG("Unsupported second-level CFE table kind: 0x%" PRIx32 " at 0x%" PRIx32, byteorder->swap32(*second_level_kind), second_level_offset);
            return PLCRASH_EINVAL;
    }

    return PLCRASH_ESUCCESS;
}

/**
 * Free all resources associated with @a reader.
 */
void plcrash_async_cfe_reader_free (plcrash_async_cfe_reader_t *reader) {
    // noop
}

/**
 * @} plcrash_async_cfe
 */