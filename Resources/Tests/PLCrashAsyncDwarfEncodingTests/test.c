#include <stdint.h>

/* 32-bit and 64-bit length headers */
struct header_32 {
    uint32_t length;
    uint32_t cie_id;
} __attribute__((packed));

struct header_64 {
    uint32_t flag64; /* Must be set to 0xffffffff */
    uint64_t length;
    uint64_t cie_id;
} __attribute__((packed));

/* Mock entry */
typedef union cfi_entry {
    struct header_32 hdr_32;
    struct header_64 hdr_64;
} cfi_entry;

// TODO
cfi_entry ef[] __attribute__((section("__PL_DWARF,__eh_frame"))) = {
    /* A mock CIE to be skipped */
    {.hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = sizeof(cfi_entry) - 12,
            .cie_id = 0,
    }},

    /* Terminator */
    {.hdr_32 = {
            .length = 0x0
    }}

    /* Additional entries after terminator -- used to test offset handling */
    // TODO
};

cfi_entry df[] __attribute__((section("__PL_DWARF,__debug_frame"))) = {
    /* A mock CIE to be skipped, using the debug_frame cie_id style */
    {.hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = sizeof(cfi_entry) - 12,
            .cie_id = UINT64_MAX,
    }},

    /* Terminator */
    {.hdr_32 = {
            .length = 0x0
    }}

 
};
