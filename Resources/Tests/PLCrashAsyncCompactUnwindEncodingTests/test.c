#include <mach-o/compact_unwind_encoding.h>
#include <stddef.h>

#define IMAGE_BASE 0x0

struct unwind_sect_compressed_page {
        struct unwind_info_compressed_second_level_page_header header;
        uint32_t entries[1];
};

struct unwind_sect_regular_page {
        struct unwind_info_regular_second_level_page_header header;
        uint32_t entries[1];
};

struct unwind_sect {
    struct unwind_info_section_header hdr;
    struct unwind_info_section_header_index_entry entries[3];

    struct unwind_sect_regular_page regular_page_1;

    struct unwind_sect_compressed_page compressed_page_1;
};

struct unwind_sect data __attribute__((section("__TEXT,__unwind_info"))) = {
    .hdr = {
        .version = 1,
        .commonEncodingsArraySectionOffset = 0,
        .commonEncodingsArrayCount = 0,
        .personalityArraySectionOffset = 0,
        .personalityArrayCount = 0,
        .indexSectionOffset = offsetof(struct unwind_sect, entries),
        .indexCount = 3 // all tools treat this as indexCount - 1
    },

    .entries = { 
        {
            .functionOffset = 0,
            .secondLevelPagesSectionOffset = offsetof(struct unwind_sect, regular_page_1)
        },
        {
            .functionOffset = 1,
            .secondLevelPagesSectionOffset = offsetof(struct unwind_sect, compressed_page_1)
        },
        { } // empty/ignored entry
    },

    .regular_page_1 = {
        .header = {
            .kind = UNWIND_SECOND_LEVEL_REGULAR,
            .entryPageOffset = offsetof(struct unwind_sect_regular_page, entries),
            .entryCount = 1
        },
        .entries = {
            0x0
        }
    },

    .compressed_page_1 = {
        .header = {
            .kind = UNWIND_SECOND_LEVEL_COMPRESSED,
            .entryPageOffset = offsetof(struct unwind_sect_compressed_page, entries),
            .entryCount = 1
        },
        .entries = {
            0x0
        },
    },
};

