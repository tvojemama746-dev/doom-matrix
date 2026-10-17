#include "input.h"

#include "hardware/gpio.h"

#define BUTTON_DOWN 0
#define BUTTON_RIGHT 1
#define BUTTON_LEFT 2
#define BUTTON_UP 3
#define BUTTON_FIRE 4

static bool previous_fire;

static void input_init_button(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}

void input_init(void) {
    input_init_button(BUTTON_DOWN);
    input_init_button(BUTTON_RIGHT);
    input_init_button(BUTTON_LEFT);
    input_init_button(BUTTON_UP);
    input_init_button(BUTTON_FIRE);
    previous_fire = false;
}

void input_update(InputState *input) {
    input->left = !gpio_get(BUTTON_LEFT);
    input->right = !gpio_get(BUTTON_RIGHT);
    input->forward = !gpio_get(BUTTON_UP);
    input->backward = !gpio_get(BUTTON_DOWN);

    bool fire_held = !gpio_get(BUTTON_FIRE);
    input->fire_pressed = fire_held && !previous_fire;
    input->fire_held = fire_held;
    previous_fire = fire_held;
}