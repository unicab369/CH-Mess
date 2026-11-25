#ifndef _FUN_SSD1306_PLOT_H
#define _FUN_SSD1306_PLOT_H

#include "fun_ssd1306.h"

typedef struct {
	const char *x_label;
	const char *y_label;
	T_Point origin;
	uint8_t x_max;
	uint8_t y_max;
	uint8_t x_tick_interval;
	uint8_t y_tick_interval;
} T_SSD1306_Plot;

// Draw a 3x3 point marker centered on the supplied coordinate.
static inline void ssd1306_plot_render_marker(T_Point point) {
	for (int8_t dy = -1; dy <= 1; ++dy) {
		for (int8_t dx = -1; dx <= 1; ++dx) {
			int16_t x = (int16_t)point.x + dx;
			int16_t y = (int16_t)point.y + dy;
			if (x >= 0 && x < SSD1306_W && y >= 0 && y < SSD1306_H)
				ssd1306_render_point((T_Point){ (uint8_t)x, (uint8_t)y });
		}
	}
}

// Draw the same clipped 3x3 marker using the filled rectangle renderer.
static inline void ssd1306_render_marker3x3(T_Point point) {
	uint8_t x_min = point.x ? point.x - 1u : 0;
	uint8_t y_min = point.y ? point.y - 1u : 0;
	uint8_t x_max = point.x + 1u < SSD1306_W ? point.x + 1u : SSD1306_W - 1u;
	uint8_t y_max = point.y + 1u < SSD1306_H ? point.y + 1u : SSD1306_H - 1u;
	ssd1306_render_rect((T_Point){ x_min, y_min }, x_max - x_min, y_max - y_min, 1);
}

// Draw a clipped 2x2 marker with point as its top-left coordinate.
static inline void ssd1306_render_marker2x2(T_Point point) {
	if (point.x >= SSD1306_W || point.y >= SSD1306_H) return;
	uint8_t width = point.x + 1u < SSD1306_W ? 1u : 0u;
	uint8_t height = point.y + 1u < SSD1306_H ? 1u : 0u;
	ssd1306_render_rect(point, width, height, 1);
}

// Render each graph point with a marker selected by dot size.
static inline void ssd1306_render_graph_markers(
	const T_Point *points, uint8_t length, uint8_t dot_size
) {
	if (!points) return;
	for (uint8_t i = 0; i < length; ++i) {
		if (dot_size <= 1u)
			ssd1306_render_point(points[i]);
		else if (dot_size == 2u)
			ssd1306_render_marker2x2(points[i]);
		else
			ssd1306_render_marker3x3(points[i]);
	}
}

// Render line segments between each consecutive pair of graph points.
static inline void ssd1306_render_graph_line(
	const T_Point *points, uint8_t length, uint8_t thickness
) {
	if (!points || length < 2u) return;
	for (uint8_t i = 1; i < length; ++i)
		ssd1306_render_line(points[i - 1u], points[i], thickness);
}

// Render plot axes and tick marks. x_max and y_max are inclusive endpoint coordinates.
static inline uint8_t ssd1306_render_plot_layout(T_SSD1306_Plot *plot) {
	if (!plot || plot->x_max <= plot->origin.x || plot->y_max <= plot->origin.y) return 0;

	uint8_t right = plot->x_max;
	uint8_t bottom = plot->y_max;

	if (plot->x_label) {
		uint16_t label_width = 0;
		for (const char *c = plot->x_label; *c; ++c)
			label_width += font_7x5.width + 1u;
		if (label_width) label_width--;

		uint16_t label_x = right + 1u > label_width ? right + 1u - label_width : 0;
		uint8_t label_y = bottom > font_7x5.height ? bottom - font_7x5.height - 1u : 0;
		ssd1306_render_str(plot->x_label, (uint8_t)label_x, label_y, &font_7x5, 1,
							SSD1306_TEXT_ORIENTATION_0);
	}

	if (plot->y_label) {
		uint16_t label_height = 0;
		for (const char *c = plot->y_label; *c; ++c)
			label_height += font_7x5.width + 1u;
		if (label_height) label_height--;
		uint16_t label_x = (uint16_t)plot->origin.x + 2u;
		uint16_t label_y = plot->origin.y;

		if (label_height && label_x + font_7x5.height <= SSD1306_W
			&& label_y + label_height <= SSD1306_H) {
			ssd1306_render_str(plot->y_label, (uint8_t)label_x, (uint8_t)label_y,
								&font_7x5, 1, SSD1306_TEXT_ORIENTATION_90);
		}
	}
	ssd1306_render_horLine(bottom, plot->origin.x, right, 1);
	ssd1306_render_verLine(plot->origin.x, plot->origin.y, bottom, 1);

	if (plot->x_tick_interval)
		for (uint16_t x = (uint16_t)plot->origin.x + plot->x_tick_interval;
			x < right; x += plot->x_tick_interval) {
			ssd1306_render_point((T_Point){ (uint8_t)x, (uint8_t)(bottom + 1u) });
			ssd1306_render_point((T_Point){ (uint8_t)x, (uint8_t)(bottom + 2u) });
		}
	if (plot->y_tick_interval) {
		for (uint16_t y = plot->origin.y; y <= bottom; y += plot->y_tick_interval) {
			ssd1306_render_point((T_Point){ (uint8_t)(plot->origin.x - 1u), (uint8_t)y });
			ssd1306_render_point((T_Point){ (uint8_t)(plot->origin.x - 2u), (uint8_t)y });
		}
	}
	return 1;
}

#endif
