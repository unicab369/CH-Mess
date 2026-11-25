# CH32V003 SSD1306 string benchmark

Compares framebuffer updates with `render_str()` and `ssd1306_drawstr_font()`
using the same 5×7 and 8×8 fonts. Both draw only lit glyph pixels and use the
font's spacing. The benchmark also times the original `ssd1306_drawstr()`, which
writes background pixels, and `display_str()`, which sends text over I2C directly.
Each result averages 25 calls per round over five rounds. The framebuffer
methods do not send data over I2C.

The display shows the 5×7 and 8×8 `render_str()` results above the matching
`ssd1306_drawstr_font()` results.
Build and flash from this directory with `make flash`.
