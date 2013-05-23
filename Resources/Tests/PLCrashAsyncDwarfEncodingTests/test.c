#include <stdint.h>

/* Mock entry */
typedef union cfi_entry {
    /* 32-bit and 64-bit size headers */
    struct {
        uint32_t size;
    } hdr_32;

    struct {
        uint32_t flag64; /* Must be set to 0xffffffff */
        uint64_t size;
    } hdr_64;
} cfi_entry;

// TODO
cfi_entry ef[] __attribute__((section("__PL_DWARF,__eh_frame"))) = {
    {.hdr_64 = {
            .flag64 = 0xffffffff,
            .size = 24,
    }},

    /* Terminator */
    {.hdr_32 = {
            .size = 0x0
    }}

    /* Additional entries after terminator -- used to test offset handling */
    // TODO
};

uint32_t df[] __attribute__((section("__PL_DWARF,__debug_frame"))) = {
    0
};
