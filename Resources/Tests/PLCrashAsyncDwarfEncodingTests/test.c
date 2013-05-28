#include <stdint.h>

#include "dwarf_encoding_test.h"

// TODO
pl_cfi_entry ef[] __attribute__((section("__PL_DWARF,__eh_frame"))) = {
    /* A mock CIE to be skipped */
    {.hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = PL_CFI_LEN_64,
            .cie_id = 0,
    }},

    /* A FDE entry */
    {.hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = PL_CFI_LEN_64,
            .cie_id = sizeof(ef[0]), // Offset to the first CIE entry
    }},

    /* Terminator */
    {.hdr_32 = {
            .length = 0x0
    }}

    /* Additional entries after terminator -- used to test offset handling */
    // TODO
};


pl_cfi_entry df[] __attribute__((section("__PL_DWARF,__debug_frame"))) = {
    /* A mock CIE to be skipped, using the debug_frame cie_id style */
    {.hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = PL_CFI_LEN_64,
            .cie_id = UINT64_MAX,
    }},

    /* A FDE entry */
    {.hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = PL_CFI_LEN_64,
            .cie_id = 0, // Offset to the first CIE entry
    }},

    /* Terminator */
    {.hdr_32 = {
            .length = 0x0
    }}
};
