#include "world.h"
#include "sound.h"
#include "pico/stdlib.h"

#include "world_map_data.h"

static const int16_t sine_quarter[65] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962,
    8739, 9512, 10278, 11039, 11793, 12539, 13279, 14010, 14732,
    15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787,
    21403, 22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832,
    26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621,
    29956, 30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757, 32767
};

#define PLAYER_RADIUS (FIXED_ONE / 5)
#define MOVE_SPEED (FIXED_ONE / 10)
#define TURN_SPEED 4
#define ENEMY_RADIUS (FIXED_ONE / 4)
#define ENEMY_MOVE_SPEED (FIXED_ONE / 28)
#define ENEMY_ATTACK_RANGE (FIXED_ONE + FIXED_ONE / 4)
#define ENEMY_ATTACK_COOLDOWN 20
#define ENEMY_ATTACK_FRAMES 5
#define ENEMY_RESPAWN_FRAMES 600
#define ENEMY_RESPAWN_MIN_DISTANCE (16 * FIXED_ONE)
#define ENEMY_INITIAL_MIN_DISTANCE (6 * FIXED_ONE)
#define PLAYER_MAX_HITS 10
#define PILLAR_REACH_STEPS 128

typedef struct {
    uint8_t damage;
    uint8_t cooldown;
    uint8_t flash_frames;
    bool automatic;
} WeaponStats;

static const WeaponStats weapons[] = {
    [WEAPON_RIFLE] = {1, 6, 3, true},
    [WEAPON_SHOTGUN] = {4, 24, 5, false},
    [WEAPON_SMG] = {1, 3, 2, true},
};

static uint32_t random_state = 2463534242u;

static uint32_t world_random_next(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

int16_t world_sin(uint8_t angle) {
    uint8_t quadrant = angle >> 6;
    uint8_t index = angle & 63u;

    switch (quadrant) {
    case 0:
        return sine_quarter[index];
    case 1:
        return sine_quarter[64u - index];
    case 2:
        return (int16_t)-sine_quarter[index];
    default:
        return (int16_t)-sine_quarter[64u - index];
    }
}

int16_t world_cos(uint8_t angle) {
    return world_sin((uint8_t)(angle + 64u));
}

bool world_is_wall(int tile_x, int tile_y) {
    return world_tile_type(tile_x, tile_y) != '.';
}

uint8_t world_tile_type(int tile_x, int tile_y) {
    if (tile_x < 0 || tile_x >= WORLD_MAP_SIZE || tile_y < 0 || tile_y >= WORLD_MAP_SIZE) {
        return '#';
    }
    return world_map[tile_y][tile_x];
}

static int32_t world_abs(int32_t value) {
    return value < 0 ? -value : value;
}

static bool world_position_clear_radius(int32_t x, int32_t y, int32_t radius) {
    return !world_is_wall((x - radius) >> 16, (y - radius) >> 16)
        && !world_is_wall((x + radius) >> 16, (y - radius) >> 16)
        && !world_is_wall((x - radius) >> 16, (y + radius) >> 16)
        && !world_is_wall((x + radius) >> 16, (y + radius) >> 16);
}

static bool world_position_clear(int32_t x, int32_t y) {
    return world_position_clear_radius(x, y, PLAYER_RADIUS);
}

static bool world_in_hub(int32_t x, int32_t y) {
    return (x >> 16) < HUB_SIZE && (y >> 16) < HUB_SIZE;
}

static bool world_enemy_position_clear(const World *world, int enemy_index,
                                       int32_t x, int32_t y) {
    if (!world_position_clear_radius(x, y, ENEMY_RADIUS)) {
        return false;
    }
    if (world_in_hub(x - ENEMY_RADIUS, y - ENEMY_RADIUS)) {
        return false;
    }
    if (world_abs(x - world->player_x) < PLAYER_RADIUS + ENEMY_RADIUS
        && world_abs(y - world->player_y) < PLAYER_RADIUS + ENEMY_RADIUS) {
        return false;
    }
    for (int index = 0; index < WORLD_ENEMY_COUNT; ++index) {
        const Enemy *enemy = &world->enemies[index];
        if (index == enemy_index || !enemy->alive) {
            continue;
        }
        if (world_abs(x - enemy->x) < ENEMY_RADIUS * 2
            && world_abs(y - enemy->y) < ENEMY_RADIUS * 2) {
            return false;
        }
    }
    return true;
}

static bool world_line_clear(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y) {
    int32_t delta_x = to_x - from_x;
    int32_t delta_y = to_y - from_y;
    int32_t largest_delta = world_abs(delta_x);
    int32_t vertical_delta = world_abs(delta_y);
    if (vertical_delta > largest_delta) {
        largest_delta = vertical_delta;
    }
    if (largest_delta == 0) {
        return true;
    }

    int steps = (largest_delta + FIXED_ONE / 4 - 1) / (FIXED_ONE / 4);
    for (int step = 1; step <= steps; ++step) {
        int32_t sample_x = from_x + (int32_t)(((int64_t)delta_x * step) / steps);
        int32_t sample_y = from_y + (int32_t)(((int64_t)delta_y * step) / steps);
        if (world_is_wall(sample_x >> 16, sample_y >> 16)) {
            return false;
        }
    }
    return true;
}

static bool world_enemy_in_sights(const World *world, const Enemy *enemy) {
    int32_t delta_x = enemy->x - world->player_x;
    int32_t delta_y = enemy->y - world->player_y;
    int32_t forward = (int32_t)(((int64_t)delta_x * world_cos(world->angle)
        + (int64_t)delta_y * world_sin(world->angle)) >> 15);
    int32_t side = (int32_t)((-(int64_t)delta_x * world_sin(world->angle)
        + (int64_t)delta_y * world_cos(world->angle)) >> 15);

    return forward > 0 && world_abs(side) <= forward / 4
        && world_line_clear(world->player_x, world->player_y, enemy->x, enemy->y);
}

static void world_update_enemy(World *world, int enemy_index) {
    Enemy *enemy = &world->enemies[enemy_index];
    if (!enemy->alive) {
        return;
    }
    if (enemy->attack_cooldown > 0) {
        --enemy->attack_cooldown;
    }
    if (enemy->attack_frames > 0) {
        --enemy->attack_frames;
    }

    int32_t delta_x = world->player_x - enemy->x;
    int32_t delta_y = world->player_y - enemy->y;
    int32_t distance = world_abs(delta_x) + world_abs(delta_y);
    if (distance <= ENEMY_ATTACK_RANGE) {
        if (enemy->attack_cooldown == 0 && !world_in_hub(world->player_x, world->player_y)) {
            enemy->attack_frames = ENEMY_ATTACK_FRAMES;
            if (world->player_hit_frames == 0 && world->player_hits < PLAYER_MAX_HITS) {
                ++world->player_hits;
                if (world->player_hits >= PLAYER_MAX_HITS) {
                    world->player_dead = true;
                }
            }
            world->player_hit_frames = ENEMY_ATTACK_FRAMES;
            enemy->attack_cooldown = ENEMY_ATTACK_COOLDOWN;
        }
        return;
    }
    if (!world_line_clear(enemy->x, enemy->y,
            world->player_x, world->player_y)) {
        return;
    }

    int32_t largest_delta = world_abs(delta_x);
    int32_t vertical_delta = world_abs(delta_y);
    if (vertical_delta > largest_delta) {
        largest_delta = vertical_delta;
    }
    int32_t move_x = (int32_t)(((int64_t)delta_x * ENEMY_MOVE_SPEED) / largest_delta);
    int32_t move_y = (int32_t)(((int64_t)delta_y * ENEMY_MOVE_SPEED) / largest_delta);
    if (world_enemy_position_clear(world, enemy_index, enemy->x + move_x, enemy->y)) {
        enemy->x += move_x;
    }
    if (world_enemy_position_clear(world, enemy_index, enemy->x, enemy->y + move_y)) {
        enemy->y += move_y;
    }
}

static bool world_spawn_enemy(World *world, int index, int32_t min_distance) {
    Enemy *enemy = &world->enemies[index];

    for (int attempt = 0; attempt < 64; ++attempt) {
        int tile_x = 1 + (int)(world_random_next(&random_state) % (WORLD_MAP_SIZE - 2));
        int tile_y = 1 + (int)(world_random_next(&random_state) % (WORLD_MAP_SIZE - 2));
        int32_t x = (tile_x << 16) + FIXED_ONE / 2;
        int32_t y = (tile_y << 16) + FIXED_ONE / 2;
        if (world_abs(x - world->player_x) + world_abs(y - world->player_y) < min_distance
            || !world_enemy_position_clear(world, index, x, y)) {
            continue;
        }

        uint32_t roll = world_random_next(&random_state) % 100u;
        if (roll < 50u) {
            enemy->type = ENEMY_TYPE_WEAK;
            enemy->hit_points = 5;
        } else if (roll < 85u) {
            enemy->type = ENEMY_TYPE_NORMAL;
            enemy->hit_points = 10;
        } else {
            enemy->type = ENEMY_TYPE_ARMORED;
            enemy->hit_points = 10;
        }
        enemy->x = x;
        enemy->y = y;
        enemy->alive = true;
        enemy->attack_frames = 0;
        enemy->attack_cooldown = 0;
        enemy->respawn_timer = 0;
        return true;
    }
    return false;
}

static void world_update_enemies(World *world) {
    for (int index = 0; index < WORLD_ENEMY_COUNT; ++index) {
        Enemy *enemy = &world->enemies[index];
        if (enemy->alive) {
            world_update_enemy(world, index);
        } else if (enemy->respawn_timer > 0) {
            --enemy->respawn_timer;
        } else {
            world_spawn_enemy(world, index, ENEMY_RESPAWN_MIN_DISTANCE);
        }
    }
}

static void world_place_player(World *world) {
    world->player_x = 3 * FIXED_ONE + FIXED_ONE / 2;
    world->player_y = 4 * FIXED_ONE + FIXED_ONE / 2;
    world->angle = 192;
    world->fire_frames = 0;
    world->fire_cooldown = 0;
    world->player_hit_frames = 0;
    world->player_hits = 0;
    world->player_dead = false;
    world->weapon_type = WEAPON_RIFLE;
}

void world_init(World *world) {
    random_state = time_us_32() | 1u;
    world_place_player(world);
    for (int index = 0; index < WORLD_ENEMY_COUNT; ++index) {
        world->enemies[index].alive = false;
        world->enemies[index].respawn_timer = 0;
        world_spawn_enemy(world, index, ENEMY_INITIAL_MIN_DISTANCE);
    }
}

static void world_shoot_pillar(World *world) {
    // cos/sin are Q15, so >> 2 gives a 1/8 tile step in Q16.
    int32_t step_x = (int32_t)world_cos(world->angle) >> 2;
    int32_t step_y = (int32_t)world_sin(world->angle) >> 2;
    int32_t x = world->player_x;
    int32_t y = world->player_y;

    for (int step = 0; step < PILLAR_REACH_STEPS; ++step) {
        x += step_x;
        y += step_y;
        uint8_t tile = world_tile_type(x >> 16, y >> 16);
        if (tile == '.') {
            continue;
        }
        if (tile == 'Y') {
            world->weapon_type = WEAPON_SHOTGUN;
        } else if (tile == 'B') {
            world->weapon_type = WEAPON_SMG;
        } else if (tile == 'W') {
            world->weapon_type = WEAPON_RIFLE;
        }
        return;
    }
}

static void world_shoot_enemy(World *world, uint8_t damage) {
    int target_index = -1;
    int32_t nearest_target = INT32_MAX;

    for (int index = 0; index < WORLD_ENEMY_COUNT; ++index) {
        Enemy *enemy = &world->enemies[index];
        if (!enemy->alive || !world_enemy_in_sights(world, enemy)) {
            continue;
        }
        int32_t delta_x = enemy->x - world->player_x;
        int32_t delta_y = enemy->y - world->player_y;
        int32_t forward = (int32_t)(((int64_t)delta_x * world_cos(world->angle)
            + (int64_t)delta_y * world_sin(world->angle)) >> 15);
        if (forward < nearest_target) {
            nearest_target = forward;
            target_index = index;
        }
    }
    if (target_index < 0) {
        return;
    }

    Enemy *target = &world->enemies[target_index];
    if (target->type == ENEMY_TYPE_ARMORED && (world_random_next(&random_state) & 1u)) {
        sound_play_enemy_hit(false);
        return;
    }
    if (target->hit_points > damage) {
        target->hit_points = (uint8_t)(target->hit_points - damage);
        sound_play_enemy_hit(false);
        return;
    }
    target->hit_points = 0;
    target->alive = false;
    target->respawn_timer = ENEMY_RESPAWN_FRAMES;
    sound_play_enemy_kill();
}

void world_update(World *world, const InputState *input) {
    if (world->player_dead) {
        world_place_player(world);
        return;
    }
    if (world->fire_frames > 0) {
        --world->fire_frames;
    }
    if (world->fire_cooldown > 0) {
        --world->fire_cooldown;
    }
    if (world->player_hit_frames > 0) {
        --world->player_hit_frames;
    }
    if (world_in_hub(world->player_x, world->player_y)) {
        world->player_hits = 0;
    }

    const WeaponStats *weapon = &weapons[world->weapon_type];
    bool fire_weapon = weapon->automatic ? input->fire_held : input->fire_pressed;
    if (fire_weapon && world->fire_cooldown == 0) {
        world->fire_frames = weapon->flash_frames;
        world->fire_cooldown = weapon->cooldown;
        sound_play_weapon_fire(world->weapon_type);
        if (world_in_hub(world->player_x, world->player_y)) {
            world_shoot_pillar(world);
        }
        world_shoot_enemy(world, weapon->damage);
    }

    if (input->left) {
        world->angle = (uint8_t)(world->angle - TURN_SPEED);
    }
    if (input->right) {
        world->angle = (uint8_t)(world->angle + TURN_SPEED);
    }

    int direction = (input->forward ? 1 : 0) - (input->backward ? 1 : 0);
    if (direction != 0) {
        int32_t move_x = ((int32_t)world_cos(world->angle) * MOVE_SPEED) >> 15;
        int32_t move_y = ((int32_t)world_sin(world->angle) * MOVE_SPEED) >> 15;
        if (direction < 0) {
            move_x = -move_x;
            move_y = -move_y;
        }
        if (world_position_clear(world->player_x + move_x, world->player_y)) {
            world->player_x += move_x;
        }
        if (world_position_clear(world->player_x, world->player_y + move_y)) {
            world->player_y += move_y;
        }
    }
    world_update_enemies(world);
}
