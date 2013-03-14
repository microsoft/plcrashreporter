#include <mach-o/compact_unwind_encoding.h>
#include <stddef.h>

// Keep in sync with PLCrashAsyncCompactUnwindEncodingTests
#define BASE_PC 0
#define PC_COMPACT_COMMON (BASE_PC+1)
#define PC_COMPACT_COMMON_ENCODING (UNWIND_X86_64_MODE_DWARF | 0xFF)


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
    
    uint32_t common_encodings[1];

    struct unwind_sect_regular_page regular_page_1;
    struct unwind_sect_compressed_page compressed_page_1;
};

struct unwind_sect data __attribute__((section("__TEXT,__unwind_info"))) = {
    .hdr = {
        .version = 1,
        .commonEncodingsArraySectionOffset = offsetof(struct unwind_sect, common_encodings),
        .commonEncodingsArrayCount = 1,
        .personalityArraySectionOffset = 0,
        .personalityArrayCount = 0,
        .indexSectionOffset = offsetof(struct unwind_sect, entries),
        .indexCount = 3 // all tools treat this as indexCount - 1
    },

    .entries = { 
        {
            .functionOffset = BASE_PC,
            .secondLevelPagesSectionOffset = offsetof(struct unwind_sect, regular_page_1)
        },
        {
            .functionOffset = PC_COMPACT_COMMON,
            .secondLevelPagesSectionOffset = offsetof(struct unwind_sect, compressed_page_1)
        },
        { } // empty/ignored entry
    },
    
    .common_encodings = {
        PC_COMPACT_COMMON_ENCODING,
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

#define COMPRESSED(foff, enc_index) ((foff & 0x00FFFFFF) | (enc_index << 24))

    .compressed_page_1 = {
        .header = {
            .kind = UNWIND_SECOND_LEVEL_COMPRESSED,
            .entryPageOffset = offsetof(struct unwind_sect_compressed_page, entries),
            .entryCount = 1
        },
        .entries = {
            /* Note that these are offsets from the first-level functionOffset */
            COMPRESSED(0, 0)
        },
    },
};

