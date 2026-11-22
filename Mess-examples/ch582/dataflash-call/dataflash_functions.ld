ENTRY(dataflash_sum)

SECTIONS
{
    .dataflash_sum DATAFLASH_SUM_ADDRESS : { KEEP(*(.dataflash_sum)) }
    ASSERT(SIZEOF(.dataflash_sum) <= 0x40, "sum function exceeds its 64-byte slot")

    .dataflash_callback DATAFLASH_CALLBACK_ADDRESS : { KEEP(*(.dataflash_callback)) }
    ASSERT(SIZEOF(.dataflash_callback) <= 0x40, "callback function exceeds its 64-byte slot")

    .dataflash_bytes DATAFLASH_BYTES_ADDRESS : { KEEP(*(.dataflash_bytes)) }
    ASSERT(SIZEOF(.dataflash_bytes) <= 0x40, "byte function exceeds its 64-byte slot")

    .dataflash_checksum DATAFLASH_CHECKSUM_ADDRESS : { KEEP(*(.dataflash_checksum)) }
    ASSERT(SIZEOF(.dataflash_checksum) <= 0x40, "checksum function exceeds its 64-byte slot")

    .dataflash_struct DATAFLASH_STRUCT_ADDRESS : { KEEP(*(.dataflash_struct)) }
    ASSERT(SIZEOF(.dataflash_struct) <= 0x40, "struct function exceeds its 64-byte slot")
}
