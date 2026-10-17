#include "display_ws2812.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"

#define WS2812_PIN 13
#define WS2812_BIT_CLOCK_HZ (800000u * 10u)

static uint32_t pixels[DISPLAY_PIXEL_COUNT];
static PIO pio = pio0;
static uint state_machine = 0;

void display_init(void) {
    uint offset = pio_add_program(pio, &ws2812_program);
    pio_sm_config config = ws2812_program_get_default_config(offset);

    sm_config_set_sideset_pins(&config, WS2812_PIN);
    sm_config_set_out_shift(&config, false, true, 24);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);

    uint32_t bit_clock_hz = WS2812_BIT_CLOCK_HZ;
    uint64_t divider_256 = ((uint64_t)clock_get_hz(clk_sys) * 256u
        + bit_clock_hz / 2u) / bit_clock_hz;
    sm_config_set_clkdiv_int_frac(&config, divider_256 >> 8, divider_256 & 0xffu);

    pio_gpio_init(pio, WS2812_PIN);
    pio_sm_set_consecutive_pindirs(pio, state_machine, WS2812_PIN, 1, true);
    pio_sm_init(pio, state_machine, offset, &config);
    pio_sm_set_enabled(pio, state_machine, true);
    display_clear(0);
}

void display_clear(uint32_t color) {
    for (int index = 0; index < DISPLAY_PIXEL_COUNT; ++index) {
        pixels[index] = color;
    }
}

void display_set_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }

    int physical_y = 7 - y;
    int physical_x = 15 - x;
    int led_index = physical_y * 16 + physical_x;
    pixels[led_index] = color;
}

void display_present(void) {
    for (int index = 0; index < DISPLAY_PIXEL_COUNT; ++index) {
        pio_sm_put_blocking(pio, state_machine, pixels[index]);
    }
    sleep_us(300);
}