#include "display_ws2812.h"
#include "input.h"
#include "sound.h"
#include "raycaster.h"
#include "world.h"

#include "pico/stdlib.h"

#define UPDATE_INTERVAL_MS 33

int main(void) {
    display_init();
    input_init();
    sound_init();

    World world;
    InputState input;
    world_init(&world);

    absolute_time_t next_update = get_absolute_time();
    while (true) {
        input_update(&input);
        world_update(&world, &input);
        sound_update();
        raycaster_render(&world);
        display_present();

        next_update = delayed_by_ms(next_update, UPDATE_INTERVAL_MS);
        sleep_until(next_update);
    }
}