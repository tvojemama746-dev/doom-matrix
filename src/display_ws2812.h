#ifndef DISPLAY_WS2812_H
#define DISPLAY_WS2812_H

#include <stdint.h>

#define DISPLAY_WIDTH 16
#define DISPLAY_HEIGHT 8
#define DISPLAY_PIXEL_COUNT (DISPLAY_WIDTH * DISPLAY_HEIGHT)

void display_init(void);
void display_clear(uint32_t color);
void display_set_pixel(int x, int y, uint32_t color);
void display_present(void);

#endif