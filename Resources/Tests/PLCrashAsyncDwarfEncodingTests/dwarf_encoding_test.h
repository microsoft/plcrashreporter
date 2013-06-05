#include <stdint.h>

/* Constants and structures used to generate the CFI test binaries. See also: Resources/Tests/PLCrashAsyncDwarfEncodingTests */

struct __attribute__((packed)) pl_cie_data_64 {
    uint8_t version; /* Must be set to 1 or 3 -- 1=eh_frame, 3=DWARF3, 4=DWARF4 */
    
    uint8_t augmentation[7];
    
    uint8_t code_alignment_factor;
    uint8_t data_alignment_factor;
    uint8_t return_address_register;
    
    uint8_t augmentation_data[6];
    
    uint8_t initial_instructions[0];
};

/* 32-bit and 64-bit length headers */
struct pl_cfi_header_32 {
    uint32_t length;
    uint32_t cie_id;
} __attribute__((packed));

struct pl_cfi_header_64 {
    uint32_t flag64; /* Must be set to 0xffffffff */
    uint64_t length;
    uint64_t cie_id;
} __attribute__((packed));

/* Mock entry */
typedef struct pl_cfi_entry {
    union {
        struct pl_cfi_header_32 hdr_32;
        struct pl_cfi_header_64 hdr_64;
    };

	union {
		struct pl_cie_data_64 cie_64;
	};
} pl_cfi_entry;

/* CFE lengths, minus the initial length field. */
#define PL_CFI_LEN_64 (sizeof(pl_cfi_entry) - sizeof(uint32_t) - sizeof(uint64_t))
#define PL_CFI_LEN_32 (sizeof(pl_cfi_entry) - sizeof(uint32_t))
