#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <stdint.h>

#include "input.h"

#define WORLD_MAP_SIZE 64
#define WORLD_ENEMY_COUNT 25
#define HUB_SIZE 7
#define BUTTON_COUNT 5
#define ESCAPE_RUN_FRAMES 80
#define ESCAPE_FLASH_FRAMES 30
#define INTRO_MESSAGE "PRESS 5 BUTTONS TO ESCAPE"
// Morse for the message takes 198 units of 2 frames (396 frames); the scroll (1 column per 2 frames) finishes earlier.
#define INTRO_FRAMES 400
#define FIXED_ONE (1 << 16)

enum {
    WEAPON_RIFLE = 0,
    WEAPON_SHOTGUN = 1,
    WEAPON_SMG = 2,
};

enum {
    ENEMY_TYPE_WEAK = 0,
    ENEMY_TYPE_NORMAL = 1,
    ENEMY_TYPE_ARMORED = 2,
};

typedef struct {
    int32_t x;
    int32_t y;
    bool alive;
    uint8_t type;
    uint8_t hit_points;
    uint8_t attack_frames;
    uint8_t attack_cooldown;
    uint16_t respawn_timer;
} Enemy;

typedef struct {
    int32_t player_x;
    int32_t player_y;
    uint8_t angle;
    uint8_t fire_frames;
    uint8_t fire_cooldown;
    Enemy enemies[WORLD_ENEMY_COUNT];
    uint8_t player_hit_frames;
    uint8_t player_hits;
    bool player_dead;
    uint8_t weapon_type;
    int32_t button_x[BUTTON_COUNT];
    int32_t button_y[BUTTON_COUNT];
    bool button_pressed[BUTTON_COUNT];
    uint8_t button_target;
    uint8_t win_frames;
    uint8_t test_button_frames;
    uint16_t intro_frames;
    uint8_t escape_frames;
    uint8_t view_bob;
} World;

void world_init(World *world);
void world_update(World *world, const InputState *input);
bool world_is_wall(int tile_x, int tile_y);
uint8_t world_tile_type(int tile_x, int tile_y);
int16_t world_sin(uint8_t angle);
int16_t world_cos(uint8_t angle);

#endif
