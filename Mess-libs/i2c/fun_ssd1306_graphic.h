// MIT License
// Copyright (c) 2026 unicab369

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#ifndef _FUN_SSD1306_GRAPHIC_H
#define _FUN_SSD1306_GRAPHIC_H

#include "fun_ssd1306.h"

typedef struct {
	const char *x_label;
	const char *y_label;
	T_Point origin;
	uint8_t x_max;
	uint8_t y_max;
	uint8_t x_tick_interval;
	uint8_t y_tick_interval;
	uint8_t swap_axes;
} T_SSD1306_Plot;

static uint8_t _loading_progress;

// Draw and display new loading-bar columns only; this tracks one bar.
void ssd1306_show_progress(
	uint8_t x, uint8_t y, uint8_t height, uint8_t width, uint8_t progress
) {
	if (x >= SSD1306_W || y >= SSD1306_H || !height || !width) return;
	uint16_t max_width = SSD1306_W - x;
	uint16_t max_height = SSD1306_H - y;
	if (width > max_width) width = max_width;
	if (height > max_height) height = max_height;
	if (progress > width) progress = width;

	if (progress < _loading_progress) return;

	uint16_t x_start = (uint16_t)x + _loading_progress;
	uint16_t x_end = (uint16_t)x + progress;
	uint8_t y_end = y + height - 1u;

	for (uint16_t x_pos = x_start; x_pos < x_end; ++x_pos)
		ssd1306_render_verLine((uint8_t)x_pos, y, y_end, 1);

	if (progress > _loading_progress) {
		uint8_t first_page = y >> 3;
		uint8_t last_page = (y + height - 1u) >> 3;
		ssd1306_displayArea(first_page, last_page, (uint8_t)x_start, (uint8_t)x_end);
	}
	_loading_progress = progress;
}

void ssd1306_reset_progress(uint8_t x, uint8_t y, uint8_t height) {
	if (_loading_progress) {
		uint16_t x_end = (uint16_t)x + _loading_progress - 1u;
		uint8_t y_end = y + height - 1u;
		// Toggle the existing filled columns off in the framebuffer.
		ssd1306_toggle_invert();
		for (uint16_t x_pos = x; x_pos <= x_end; ++x_pos)
			ssd1306_render_verLine((uint8_t)x_pos, y, y_end, 1);
		ssd1306_toggle_invert();

		uint8_t first_page = y >> 3;
		uint8_t last_page = (y + height - 1u) >> 3;
		// Refresh only the cleared columns on the affected pages.
		ssd1306_displayArea(first_page, last_page, x, (uint8_t)(x_end + 1u));
	}
	_loading_progress = 0;
}

// Convert a percentage from 0 to 100 into pixel progress for the loading bar.
void ssd1306_show_percent(
	uint8_t x, uint8_t y, uint8_t height, uint8_t width, uint8_t percent
) {
	if (percent > 100u) percent = 100u;
	uint8_t progress = (uint16_t)width * percent / 100u;
	ssd1306_show_progress(x, y, height, width, progress);
}

// Draw the same clipped 3x3 marker using the filled rectangle renderer.
void ssd1306_render_marker3x3(uint8_t x, uint8_t y) {
	if (x >= SSD1306_W || y >= SSD1306_H) return;
	uint8_t x_min = x ? x - 1u : 0;
	uint8_t y_min = y ? y - 1u : 0;
	uint16_t x_next = (uint16_t)x + 1u;
	uint16_t y_next = (uint16_t)y + 1u;
	uint8_t x_max = x_next < SSD1306_W ? (uint8_t)x_next : SSD1306_W - 1u;
	uint8_t y_max = y_next < SSD1306_H ? (uint8_t)y_next : SSD1306_H - 1u;

	for (uint16_t x = x_min; x <= x_max; ++x)
		ssd1306_render_verLine((uint8_t)x, y_min, y_max, 1);
}

// Draw a clipped 2x2 marker with point as its top-left coordinate.
void ssd1306_render_marker2x2(uint8_t x, uint8_t y) {
	if (x >= SSD1306_W || y >= SSD1306_H) return;
	uint16_t x_next = (uint16_t)x + 1u;
	uint16_t y_next = (uint16_t)y + 1u;
	uint8_t x_end = x_next < SSD1306_W ? (uint8_t)x_next : x;
	uint8_t y_end = y_next < SSD1306_H ? (uint8_t)y_next : y;

	for (uint16_t x_pos = x; x_pos <= x_end; ++x_pos)
		ssd1306_render_verLine((uint8_t)x_pos, y, y_end, 1);
}

// Convert graph values (x from the y-axis, y above the x-axis) to screen pixels.
uint8_t ssd1306_plot_to_screen(
	const T_SSD1306_Plot *plot, uint8_t x, uint8_t y, uint8_t *screen_x, uint8_t *screen_y
) {
	if (!plot || !screen_x || !screen_y || plot->x_max <= plot->origin.x
		|| plot->y_max <= plot->origin.y
		|| plot->x_max >= SSD1306_W || plot->y_max >= SSD1306_H
		|| (plot->swap_axes
			? (y > plot->x_max - plot->origin.x || x > plot->y_max - plot->origin.y)
			: (x > plot->x_max - plot->origin.x || y > plot->y_max - plot->origin.y))
	) return 0;

	if (plot->swap_axes) {
		*screen_x = plot->origin.x + y;
		*screen_y = plot->y_max - x;
	} else {
		*screen_x = plot->origin.x + x;
		*screen_y = plot->y_max - y;
	}
	return 1;
}

// Render each graph value with a marker selected by dot size.
void ssd1306_render_graph_markers(
	const T_SSD1306_Plot *plot, const T_Point *points, uint8_t length, uint8_t dot_size
) {
	if (!plot || !points) return;
	for (uint8_t i = 0; i < length; ++i) {
		uint8_t screen_x, screen_y;
		if (!ssd1306_plot_to_screen(plot, points[i].x, points[i].y, &screen_x, &screen_y))
			continue;
		if (dot_size <= 1u)
			ssd1306_render_point(screen_x, screen_y);
		else if (dot_size == 2u)
			ssd1306_render_marker2x2(screen_x, screen_y);
		else
			ssd1306_render_marker3x3(screen_x, screen_y);
	}
}

// Render line segments between each consecutive pair of graph points.
void ssd1306_render_graph_line(
	const T_SSD1306_Plot *plot, const T_Point *points,
	uint8_t length, uint8_t thickness
) {
	if (!plot || !points || length < 2u) return;
	for (uint8_t i = 1; i < length; ++i) {
		uint8_t x0, y0, x1, y1;

		if (ssd1306_plot_to_screen(plot, points[i - 1u].x, points[i - 1u].y, &x0, &y0)
			&& ssd1306_plot_to_screen(plot, points[i].x, points[i].y, &x1, &y1)
		) {
			ssd1306_render_line((T_Point){ x0, y0 }, (T_Point){ x1, y1 }, thickness);
		}
	}
}

// Draw filled bars from each graph value down to the plot's x-axis.
void ssd1306_render_graph_bars(
	const T_SSD1306_Plot *plot, const T_Point *points, uint8_t length, uint8_t bar_width
) {
	if (!plot || !points || !bar_width) return;

	if (plot->swap_axes) {
		for (uint8_t i = 0; i < length; ++i) {
			uint8_t screen_x, screen_y;
			if (!ssd1306_plot_to_screen(plot, points[i].x, points[i].y, &screen_x, &screen_y))
				continue;

			uint16_t width = screen_x - plot->origin.x + 1u;
			uint16_t height = bar_width;
			uint16_t plot_height = (uint16_t)plot->y_max - screen_y + 1u;
			if (height > plot_height) height = plot_height;
			T_Point bar_origin = { plot->origin.x, screen_y };
			ssd1306_render_rect(bar_origin, (uint8_t)(width - 1u), (uint8_t)(height - 1u), 1);
		}
	} else {
		for (uint8_t i = 0; i < length; ++i) {
			uint8_t screen_x, screen_y;
			if (!ssd1306_plot_to_screen(plot, points[i].x, points[i].y, &screen_x, &screen_y))
				continue;

			uint16_t width = bar_width;
			uint16_t plot_width = (uint16_t)plot->x_max - screen_x + 1u;
			if (width > plot_width) width = plot_width;
			uint8_t height = plot->y_max - screen_y + 1u;
			ssd1306_render_rect((T_Point){ screen_x, screen_y },
				(uint8_t)(width - 1u), (uint8_t)(height - 1u), 1);
		}
	}
}

// Fill the area between consecutive graph points and the plot's x-axis.
void ssd1306_render_graph_fill(
	const T_SSD1306_Plot *plot, const T_Point *points, uint8_t length
) {
	if (!plot || !points || length < 2u) return;
	for (uint8_t i = 1; i < length; ++i) {
		uint8_t x0, y0, x1, y1;
		if (!ssd1306_plot_to_screen(plot, points[i - 1u].x, points[i - 1u].y, &x0, &y0)
			|| !ssd1306_plot_to_screen(plot, points[i].x, points[i].y, &x1, &y1)
		) continue;

		T_Point segment[4] = {
			{ x0, y0 }, { x1, y1 },
			{ x1, plot->y_max },
			{ x0, plot->y_max },
		};
		ssd1306_render_solid_poly(segment, 4);
	}
}

// Render plot axes and tick marks. x_max and y_max are inclusive endpoint coordinates.
uint8_t ssd1306_render_plot_layout(T_SSD1306_Plot *plot) {
	if (!plot || plot->x_max <= plot->origin.x || plot->y_max <= plot->origin.y) return 0;
	const char *h_label = plot->swap_axes ? plot->y_label : plot->x_label;
	const char *v_label = plot->swap_axes ? plot->x_label : plot->y_label;

	uint8_t right = plot->x_max;
	uint8_t bottom = plot->y_max;

	if (h_label) {
		uint16_t label_width = 0;
		for (const char *c = h_label; *c; ++c)
			label_width += font_7x5.width + 1u;
		if (label_width) label_width--;

		uint16_t label_x = right + 1u > label_width ? right + 1u - label_width : 0;
		uint8_t label_y = bottom > font_7x5.height ? bottom - font_7x5.height - 1u : 0;
		ssd1306_render_str(h_label, (uint8_t)label_x, label_y, &font_7x5, 1,
							TEXT_ORIENTATION_0);
	}

	if (v_label) {
		uint16_t label_height = 0;
		for (const char *c = v_label; *c; ++c)
			label_height += font_7x5.width + 1u;
		if (label_height) label_height--;
		uint16_t label_x = (uint16_t)plot->origin.x + 2u;
		uint16_t label_y = plot->origin.y;

		if (label_height && label_x + font_7x5.height <= SSD1306_W
			&& label_y + label_height <= SSD1306_H) {
			ssd1306_render_str(v_label, (uint8_t)label_x, (uint8_t)label_y,
								&font_7x5, 1, TEXT_ORIENTATION_90);
		}
	}
	ssd1306_render_horLine(bottom, plot->origin.x, right, 1);
	ssd1306_render_verLine(plot->origin.x, plot->origin.y, bottom, 1);

	uint8_t h_tick = plot->swap_axes ? plot->y_tick_interval : plot->x_tick_interval;
	uint8_t v_tick = plot->swap_axes ? plot->x_tick_interval : plot->y_tick_interval;
	if (h_tick)
		for (uint16_t x = (uint16_t)plot->origin.x + h_tick;
			x < right; x += h_tick) {
			ssd1306_render_point((uint8_t)x, (uint8_t)(bottom + 1u));
			ssd1306_render_point((uint8_t)x, (uint8_t)(bottom + 2u));
		}
	if (v_tick) {
		for (uint16_t y = plot->origin.y; y <= bottom; y += v_tick) {
			ssd1306_render_point((uint8_t)(plot->origin.x - 1u), (uint8_t)y);
			ssd1306_render_point((uint8_t)(plot->origin.x - 2u), (uint8_t)y);
		}
	}
	return 1;
}

#endif
