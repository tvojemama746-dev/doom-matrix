#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

typedef struct {
    bool left;
    bool right;
    bool forward;
    bool backward;
    bool fire_pressed;
    bool fire_held;
} InputState;

void input_init(void);
void input_update(InputState *input);

#endif