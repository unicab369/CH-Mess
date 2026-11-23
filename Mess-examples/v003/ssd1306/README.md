# CH32V003 SSD1306 vertical-line benchmark

This example compares the time to draw the same 56-pixel vertical line using
`prefill_verLine()` and `ssd1306_drawFastVLine()`. It runs 1,000 lines per
round for five rounds per function, measuring each function in a separate
phase, and prints the average SysTick ticks per call over the serial/debug
output.

The benchmark configures both functions to use the same frame buffer, which
keeps RAM use low and ensures they write to the same display memory.

The SSD1306 module uses I2C address `0x3C` and the library's default pins:
PC2 for SCL and PC1 for SDA. Build and flash from this directory with
`make flash`.
