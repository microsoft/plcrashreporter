#include <stdint.h>

/* Constants used to generate the CFI test binaries. See also: Resources/Tests/PLCrashAsyncDwarfEncodingTests */

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
} pl_cfi_entry;

/* CFE lengths, minus the initial length field. */
#define PL_CFI_LEN_64 (sizeof(pl_cfi_entry) - sizeof(uint32_t) - sizeof(uint64_t))
#define PL_CFI_LEN_32 (sizeof(pl_cfi_entry) - sizeof(uint32_t))
