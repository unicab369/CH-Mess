# CH32V003 SSD1306 string benchmark

Times framebuffer updates with `render_str()` using the 5×7 and 8×8 fonts.
It benchmarks `render_str_scaled()` at 2× with both fonts using eight characters.
It also times `render_str_line()` followed by `ssd1306_displayArea()`.
`render_str_line()` copies up to 25 five-column glyphs into one framebuffer
page without sending them. The area transfer sends 125 bytes over I2C.

The display shows the 5×7 and 8×8 `render_str()` results above their 2×
scaled versions.
Build and flash from this directory with `make flash`.
