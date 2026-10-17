#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <stdint.h>

#include "input.h"

#define WORLD_MAP_SIZE 20
#define WORLD_ENEMY_COUNT 10
#define FIXED_ONE (1 << 16)

typedef struct {
    int32_t x;
    int32_t y;
    bool alive;
    uint8_t hit_points;
    uint8_t attack_frames;
    uint8_t attack_cooldown;
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
    bool in_menu;
    bool startup_menu;
    uint8_t startup_menu_selection;
    bool weapon_menu;
    uint8_t weapon_menu_selection;
    uint8_t game_mode;
    uint8_t level;
    uint8_t level_enemy_count;
    uint8_t level_enemy_hit_points;
    uint8_t weapon_type;
    uint8_t menu_tap_count;
    uint8_t menu_tap_timer;
} World;

void world_init(World *world);
void world_update(World *world, const InputState *input);
bool world_is_wall(int tile_x, int tile_y);
uint8_t world_tile_type(int tile_x, int tile_y);
int16_t world_sin(uint8_t angle);
int16_t world_cos(uint8_t angle);

#endif