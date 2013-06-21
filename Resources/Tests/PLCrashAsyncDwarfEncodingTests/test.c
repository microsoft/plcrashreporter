#include <stdint.h>

#include "dwarf_encoding_test.h"

// TODO
pl_cfi_entry ef[] __attribute__((section("__PL_DWARF,__eh_frame"))) = {
    /* Common CIE entry */
    {
        .hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = PL_CFI_LEN_64,
            .cie_id = 0,
        },
        
        .cie_64 = {
            .version = 1, // eh_frame
            .augmentation[0] = 'z', // Enable GNU EH augmentation handling
            .augmentation[1] = 'R', // Pointer encoding is included in the augmentation data.
            .augmentation[2] = '\0',
            
            .code_alignment_factor = 0,
            .data_alignment_factor = 0,
            .return_address_register = 0,
            
            .augmentation_data[0] = sizeof(ef[0].cie_64.augmentation_data), // augmentation data length; uleb128, must fit in 7 bits.
            .augmentation_data[1] = 0x04, // DW_EH_PE_udata8 FDE pointer size
        }
    },

    /* A FDE entry */
    {
        .hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = PL_CFI_LEN_64,
            .cie_id = sizeof(ef[0]), // Offset to the first CIE entry
        },
        .fde_64 = {
            .initial_location = PL_CFI_EH_FRAME_PC,
            .address_range = PL_CFI_EH_FRAME_PC_RANGE
        }
    },

    /* Terminator */
    {.hdr_32 = {
            .length = 0x0
    }}

    /* Additional entries after terminator -- used to test offset handling */
    // TODO
};


pl_cfi_entry df[] __attribute__((section("__PL_DWARF,__debug_frame"))) = {
    /* Common CIE entry */
    {
        .hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = PL_CFI_LEN_64,
            .cie_id = UINT64_MAX,
        },
        
        .cie_64 = {
            .version = 1, // eh_frame
            .augmentation[0] = 'z',
            .augmentation[1] = 'R',
            .augmentation[2] = '\0',
            
            .code_alignment_factor = 0,
            .data_alignment_factor = 0,
            .return_address_register = 0,
            
            .augmentation_data[0] = sizeof(ef[0].cie_64.augmentation_data), // uleb128, must fit in 7 bits.
            .augmentation_data[1] = 0x04, // DW_EH_PE_udata8 FDE pointer size
        }
    },

    /* A FDE entry */
    {
        .hdr_64 = {
            .flag64 = UINT32_MAX,
            .length = PL_CFI_LEN_64,
            .cie_id = 0, // Offset to the first CIE entry
        },
        .fde_64 = {
            .initial_location = PL_CFI_DEBUG_FRAME_PC,
            .address_range = PL_CFI_DEBUG_FRAME_PC_RANGE
        }
    },

    /* Terminator */
    {.hdr_32 = {
            .length = 0x0
    }}
};
