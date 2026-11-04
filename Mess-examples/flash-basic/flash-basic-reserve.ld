INCLUDE ../../ch32fun/ch32fun/generated_CH582_.ld

ASSERT(
    LOADADDR(.data) + SIZEOF(.data) <= 0x00002000,
    "flash-basic CodeFlash image overlaps the reserved log sectors at 0x2000"
)
