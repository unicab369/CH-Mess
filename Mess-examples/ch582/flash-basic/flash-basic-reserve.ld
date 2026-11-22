__flash_log_start = ORIGIN(FLASH) + LENGTH(FLASH) - __flash_log_size;

ASSERT(
    LOADADDR(.data) + SIZEOF(.data) <= __flash_log_start,
    "flash-basic CodeFlash image overlaps the reserved log blocks"
)

/* Erasing the log clears a whole block. Start it at a block edge so firmware
   bytes are not erased with it. */
ASSERT(
    (__flash_log_start % __flash_block_size) == 0,
    "flash-basic log start is not erase-block aligned"
)
