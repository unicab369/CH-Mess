#ifndef _FUN_SSD1306_PLOT_H
#define _FUN_SSD1306_PLOT_H

#include "fun_ssd1306.h"

typedef struct {
	const char *x_label;
	const char *y_label;
	T_Point origin;
	uint8_t x_max;
	uint8_t y_max;
	uint8_t x_tick;
	uint8_t y_tick;
} T_SSD1306_Plot;

// Render plot axes and tick marks. x_max and y_max are inclusive endpoint coordinates.
static inline uint8_t ssd1306_plot_render_layout(T_SSD1306_Plot *plot) {
	if (!plot || plot->x_max <= plot->origin.x || plot->y_max <= plot->origin.y) return 0;

	uint8_t right = plot->x_max;
	uint8_t bottom = plot->y_max;

	if (plot->x_label) {
		uint16_t label_width = 0;
		for (const char *c = plot->x_label; *c; ++c)
			label_width += font_7x5.width + font_7x5.spacing;
		if (label_width) label_width -= font_7x5.spacing;

		uint16_t label_x = right + 1u > label_width ? right + 1u - label_width : 0;
		uint8_t label_y = bottom > font_7x5.height ? bottom - font_7x5.height - 1u : 0;
		ssd1306_render_str(plot->x_label, (uint8_t)label_x, label_y, &font_7x5, 1);
	}

	if (plot->y_label) {
		uint16_t label_height = 0;
		for (const char *c = plot->y_label; *c; ++c)
			label_height += font_7x5.height;
		uint16_t label_x = (uint16_t)plot->origin.x + 2u;
		uint16_t label_y = plot->origin.y;

		if (label_height && label_x + font_7x5.width <= SSD1306_W
			&& label_y + label_height <= SSD1306_H) {
			for (const char *c = plot->y_label; *c; ++c) {
				char glyph[2] = { *c, '\0' };
				ssd1306_render_str(glyph, (uint8_t)label_x, (uint8_t)label_y, &font_7x5, 0);
				label_y += font_7x5.height;
			}
		}
	}
	ssd1306_render_horLine(bottom, plot->origin.x, right, 1);
	ssd1306_render_verLine(plot->origin.x, plot->origin.y, bottom, 1);

	if (plot->x_tick)
		for (uint16_t x = (uint16_t)plot->origin.x + plot->x_tick; x < right; x += plot->x_tick) {
			ssd1306_render_point((T_Point){ (uint8_t)x, (uint8_t)(bottom + 1u) });
			ssd1306_render_point((T_Point){ (uint8_t)x, (uint8_t)(bottom + 2u) });
		}
	if (plot->y_tick) {
		for (uint16_t y = plot->origin.y; y <= bottom; y += plot->y_tick) {
			ssd1306_render_point((T_Point){ (uint8_t)(plot->origin.x - 1u), (uint8_t)y });
			ssd1306_render_point((T_Point){ (uint8_t)(plot->origin.x - 2u), (uint8_t)y });
		}
	}
	return 1;
}

#endif
