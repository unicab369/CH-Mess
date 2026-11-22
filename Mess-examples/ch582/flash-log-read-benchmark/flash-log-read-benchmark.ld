__flash_log_start = ORIGIN(FLASH) + LENGTH(FLASH) - __flash_log_size;

ASSERT(
    LOADADDR(.data) + SIZEOF(.data) <= __flash_log_start,
    "flash-log-read-benchmark firmware overlaps the reserved log blocks"
)

/* Keep the reserved log region on erase-block boundaries. */
ASSERT(
    (__flash_log_start % __flash_block_size) == 0,
    "flash-log-read-benchmark log start is not erase-block aligned"
)
