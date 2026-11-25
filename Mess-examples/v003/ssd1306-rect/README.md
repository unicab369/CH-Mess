# CH32V003 SSD1306 rectangle benchmark

This example benchmarks outline and filled rectangles with `render_rect()`.
Each mode draws 500 32×24 rectangles per round for five rounds, then prints
average SysTick ticks per rectangle. Filled rectangles use vertical lines to
match the SSD1306 framebuffer's page layout.

`render_rect()` uses inclusive end coordinates, so the benchmark passes
width minus one and height minus one as its width and height offsets. The
display shows the accumulated filled rectangles from the final benchmark
phase.

The SSD1306 module uses I2C address `0x3C` and the library's default pins:
PC2 for SCL and PC1 for SDA. Build and flash from this directory with
`make flash`.
