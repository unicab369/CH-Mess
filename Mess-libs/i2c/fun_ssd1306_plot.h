#ifndef _FUN_SSD1306_PLOT_H
#define _FUN_SSD1306_PLOT_H

#include "fun_ssd1306.h"

typedef struct {
	T_Point origin;
	uint8_t width;
	uint8_t height;
	uint8_t x_tick;
	uint8_t y_tick;
} T_SSD1306_Plot;

// Render plot axes and tick marks. Width and height include both boundary coordinates.
static inline uint8_t ssd1306_plot_render_layout(T_SSD1306_Plot *plot) {
	if (!plot || plot->width < 2 || plot->height < 2) return 0;

	uint8_t right = plot->origin.x + plot->width - 1u;
	uint8_t bottom = plot->origin.y + plot->height - 1u;
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
