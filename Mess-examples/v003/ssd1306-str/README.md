# CH32V003 SSD1306 string benchmark

Measures framebuffer updates only: a local `render_str_buffered()` routine
that copies the same 25 `FONT_7x5` cells used by `render_str()`, and
`ssd1306_drawstr()` using its 8×8 font. Each result averages 25 calls per round
over five rounds. `render_str()` itself sends data over I2C and does not update
the framebuffer, so it cannot be used for this buffer-only comparison. The
fonts and character widths differ.

The display shows the 7×5 font on the top page and the 8×8 font below it.
Build and flash from this directory with `make flash`.
