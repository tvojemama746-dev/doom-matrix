#include "world.h"
#include "sound.h"
#include "pico/stdlib.h"

#define WORLD_LEVEL_COUNT 6

static const uint8_t level_maps[WORLD_LEVEL_COUNT - 1][WORLD_MAP_SIZE][WORLD_MAP_SIZE] = {
    {
        "####################",
        "#..................#",
        "#.######.####.####.#",
        "#.#....#....#....#.#",
        "#.#.##.####.#.##.#.#",
        "#...##...#....#....#",
        "###.######.####.##.#",
        "#...#....#.#....##.#",
        "#.###.##.#.#.##....#",
        "#.....##...#..####.#",
        "#.##########.##....#",
        "#.#........#....##.#",
        "#.#.######.######..#",
        "#...#....#......#..#",
        "#####.##.######.#..#",
        "#.....##........#..#",
        "#.##############.#.#",
        "#..................#",
        "#..................#",
        "####################",
    },
    {
        "####################",
        "#....#........#....#",
        "#....#........#....#",
        "#....##########....#",
        "#..................#",
        "###.############.###",
        "#..................#",
        "#..####....####....#",
        "#..#..#....#..#....#",
        "#..#..#....#..#....#",
        "#..####....####....#",
        "#..................#",
        "###.############.###",
        "#..................#",
        "#....##########....#",
        "#....#........#....#",
        "#....#........#....#",
        "#..................#",
        "#..................#",
        "####################",
    },
    {
        "####################",
        "#.#.#.#.#.#.#.#.#..#",
        "#..................#",
        "#.################.#",
        "#..................#",
        "#.#.#.#.#.#.#.#.#..#",
        "#..................#",
        "#.################.#",
        "#..................#",
        "#.#.#.#.#.#.#.#.#..#",
        "#..................#",
        "#.################.#",
        "#..................#",
        "#.#.#.#.#.#.#.#.#..#",
        "#..................#",
        "#.################.#",
        "#..................#",
        "#..................#",
        "#..................#",
        "####################",
    },
    {
        "####################",
        "#..#..........#...#",
        "#..#..######..#...#",
        "#..#..#....#..#...#",
        "#..#..#....#..#...#",
        "#.....#....#......#",
        "###.####..####.###.#",
        "#...#..........#...#",
        "#...#.########.#...#",
        "#...#.#......#.#...#",
        "#...#.#.####.#.#...#",
        "#...#.#.#..#.#.#...#",
        "#...#.#.####.#.#...#",
        "#...#.#......#.#...#",
        "#...#.########.#...#",
        "#...#..........#...#",
        "###.####..####.###.#",
        "#.....#....#......#",
        "#..................#",
        "####################",
    },
    {
        "####################",
        "#....#....#....#...#",
        "#....#....#....#...#",
        "#.######..######...#",
        "#..................#",
        "#..##########.######",
        "#..#........#......#",
        "#..#........######.#",
        "#..#..####........#",
        "#..#..#..#..####..#",
        "#..#..#..#..#..#..#",
        "#..#..####..#..#..#",
        "#..#........####..#",
        "#..#........#......#",
        "#..##########.######",
        "#..................#",
        "#.######..######...#",
        "#....#....#....#...#",
        "#....#....#....#...#",
        "####################",
    },
};

static const uint8_t boss_map[WORLD_MAP_SIZE][WORLD_MAP_SIZE] = {
    "####################",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "####################",
};

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
#define FIRE_RELOAD_FRAMES 6

enum {
    WEAPON_ASSAULT_RIFLE = 0,
    WEAPON_BAZOOKA = 1,
};

static uint32_t world_random_next(uint32_t *state);
static uint8_t current_world_level = 1;
static void world_spawn_enemies(World *world);

static const uint8_t (*world_level_map(uint8_t level))[WORLD_MAP_SIZE] {
    if (level >= WORLD_LEVEL_COUNT) {
        return boss_map;
    }
    return level_maps[level - 1u];
}

static bool world_is_boss_level_level(uint8_t level) {
    return level >= 6;
}

static void world_roll_level(World *world) {
    if (world_is_boss_level_level(world->level)) {
        world->level_enemy_count = 1;
        world->level_enemy_hit_points = 50;
        return;
    }

    uint32_t random_state = time_us_32() ^ ((uint32_t)world->level * 0x9e3779b9u);
    world->level_enemy_count = 6 + (uint8_t)(world_random_next(&random_state) % 5u);
    world->level_enemy_hit_points = world->level;
}

static void world_begin_level(World *world, uint8_t level) {
    world->level = level;
    current_world_level = level;
    world->player_x = FIXED_ONE + FIXED_ONE / 2;
    world->player_y = 18 * FIXED_ONE + FIXED_ONE / 2;
    world->angle = 192;
    world->fire_frames = 0;
    world->fire_cooldown = 0;
    world->player_hit_frames = 0;
    world->player_hits = 0;
    world->player_dead = false;
    world->in_menu = false;
    world->weapon_menu = (level == 3);
    world->weapon_menu_selection = 0;
    world->menu_tap_count = 0;
    world->menu_tap_timer = 0;
    world_roll_level(world);
    world_spawn_enemies(world);
}

static void world_respawn_same_level(World *world) {
    uint8_t current_level = world->level;

    world_begin_level(world, current_level);
    world->weapon_type = WEAPON_ASSAULT_RIFLE;
    world->weapon_menu = false;
    world->weapon_menu_selection = 0;
    world->menu_tap_count = 0;
    world->menu_tap_timer = 0;
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
    return world_level_map(current_world_level)[tile_y][tile_x];
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

static uint32_t world_random_next(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static bool world_enemy_position_clear(const World *world, int enemy_index,
                                       int32_t x, int32_t y) {
    if (!world_position_clear_radius(x, y, ENEMY_RADIUS)) {
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
    int32_t attack_range = ENEMY_ATTACK_RANGE;
    int32_t move_speed = ENEMY_MOVE_SPEED;
    if (world_is_boss_level_level(world->level)) {
        attack_range += FIXED_ONE / 2;
        move_speed = ENEMY_MOVE_SPEED / 2;
    }
    if (distance <= attack_range) {
        if (enemy->attack_cooldown == 0) {
            enemy->attack_frames = ENEMY_ATTACK_FRAMES;
            if (world->player_hit_frames == 0 && world->player_hits < 10) {
                ++world->player_hits;
                if (world->player_hits >= 10) {
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
    int32_t move_x = (int32_t)(((int64_t)delta_x * move_speed) / largest_delta);
    int32_t move_y = (int32_t)(((int64_t)delta_y * move_speed) / largest_delta);
    if (world_enemy_position_clear(world, enemy_index, enemy->x + move_x, enemy->y)) {
        enemy->x += move_x;
    }
    if (world_enemy_position_clear(world, enemy_index, enemy->x, enemy->y + move_y)) {
        enemy->y += move_y;
    }
}

static void world_update_enemies(World *world) {
    for (int index = 0; index < WORLD_ENEMY_COUNT; ++index) {
        world_update_enemy(world, index);
    }
}

static void world_spawn_enemies(World *world) {
    uint32_t random_state = time_us_32() ^ ((uint32_t)world->level * 0x85ebca6bu);
    uint8_t hit_points = world->level_enemy_hit_points;

    for (int index = 0; index < WORLD_ENEMY_COUNT; ++index) {
        Enemy *enemy = &world->enemies[index];
        enemy->alive = false;
        enemy->hit_points = hit_points;
        enemy->attack_frames = 0;
        enemy->attack_cooldown = 0;
    }
    for (int index = 0; index < world->level_enemy_count; ++index) {
        Enemy *enemy = &world->enemies[index];
        enemy->hit_points = hit_points;
        for (int attempt = 0; attempt < 256; ++attempt) {
            int32_t x;
            int32_t y;
            if (world_is_boss_level_level(world->level)) {
                x = 10 * FIXED_ONE + FIXED_ONE / 2;
                y = 6 * FIXED_ONE + FIXED_ONE / 2;
            } else {
                int tile_x = 1 + (int)(world_random_next(&random_state)
                    % (WORLD_MAP_SIZE - 2));
                int tile_y = 1 + (int)(world_random_next(&random_state)
                    % (WORLD_MAP_SIZE - 2));
                x = (tile_x << 16) + FIXED_ONE / 2;
                y = (tile_y << 16) + FIXED_ONE / 2;
            }
            int32_t player_distance = world_abs(x - world->player_x)
                + world_abs(y - world->player_y);
            if (player_distance < 3 * FIXED_ONE
                || !world_enemy_position_clear(world, index, x, y)) {
                continue;
            }
            enemy->x = x;
            enemy->y = y;
            enemy->alive = true;
            break;
        }
    }
}

static void world_advance_level(World *world) {
    if (world->level < WORLD_LEVEL_COUNT) {
        world_begin_level(world, (uint8_t)(world->level + 1));
    } else {
        world_begin_level(world, 1);
    }
}

static void world_reset_game(World *world) {
    world_respawn_same_level(world);
}

void world_init(World *world) {
    world->startup_menu = false;
    world->startup_menu_selection = 0;
    world->game_mode = 0;
    world_begin_level(world, 1);
}

void world_update(World *world, const InputState *input) {
    if (world->player_dead) {
        world_reset_game(world);
        return;
    }
    if (world->weapon_menu) {
        if (input->left || input->right) {
            world->weapon_menu_selection ^= 1u;
        }
        if (input->fire_pressed) {
            world->weapon_type = world->weapon_menu_selection;
            world->weapon_menu = false;
            world->menu_tap_count = 0;
            world->menu_tap_timer = 0;
        }
        return;
    }
    if (world->in_menu) {
        if (input->left) {
            world->startup_menu_selection = (uint8_t)((world->startup_menu_selection + WORLD_LEVEL_COUNT - 1u) % WORLD_LEVEL_COUNT);
        }
        if (input->right) {
            world->startup_menu_selection = (uint8_t)((world->startup_menu_selection + 1u) % WORLD_LEVEL_COUNT);
        }
        if (input->fire_pressed) {
            world_begin_level(world, (uint8_t)(world->startup_menu_selection + 1u));
        }
        return;
    }
    if (world->menu_tap_timer > 0) {
        --world->menu_tap_timer;
    }
    if (world->fire_frames > 0) {
        --world->fire_frames;
    }
    if (world->fire_cooldown > 0) {
        --world->fire_cooldown;
    }
    if (input->fire_pressed) {
        if (world->menu_tap_timer == 0) {
            world->menu_tap_count = 0;
        }
        ++world->menu_tap_count;
        world->menu_tap_timer = 15;
        if (world->menu_tap_count >= 5) {
            world->in_menu = true;
            world->startup_menu_selection = (uint8_t)(world->level - 1u);
            world->menu_tap_count = 0;
            world->menu_tap_timer = 0;
            return;
        }
    }
    if (world->player_hit_frames > 0) {
        --world->player_hit_frames;
    }
    bool fire_weapon = world->weapon_type == WEAPON_ASSAULT_RIFLE
        ? input->fire_held
        : input->fire_pressed;
    if (fire_weapon && world->fire_cooldown == 0) {
        int target_index = -1;
        int32_t nearest_target = INT32_MAX;
        uint8_t damage = 1;
        if (world->weapon_type == WEAPON_BAZOOKA) {
            damage = 3;
            world->fire_frames = 5;
            world->fire_cooldown = 18;
        } else {
            world->fire_frames = 3;
            world->fire_cooldown = FIRE_RELOAD_FRAMES;
        }
        sound_play_weapon_fire(world->weapon_type);
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
        if (target_index >= 0) {
            Enemy *target = &world->enemies[target_index];
            bool killed = target->hit_points <= damage;
            if (target->hit_points > damage) {
                target->hit_points = (uint8_t)(target->hit_points - damage);
            } else if (target->hit_points > 0) {
                target->hit_points = 0;
                target->alive = false;
            }
            if (killed) {
                if (world_is_boss_level_level(world->level) && target_index == 0) {
                    sound_play_boss_kill();
                } else {
                    sound_play_enemy_kill();
                }
            } else {
                sound_play_enemy_hit(false);
            }
        }
    }

    if (input->left) {
        world->angle = (uint8_t)(world->angle - TURN_SPEED);
    }
    if (input->right) {
        world->angle = (uint8_t)(world->angle + TURN_SPEED);
    }

    int direction = (input->forward ? 1 : 0) - (input->backward ? 1 : 0);
    if (direction == 0) {
        world_update_enemies(world);
        return;
    }

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
    world_update_enemies(world);

    bool all_dead = true;
    for (int index = 0; index < WORLD_ENEMY_COUNT; ++index) {
        if (world->enemies[index].alive) {
            all_dead = false;
            break;
        }
    }
    if (all_dead) {
        if (world->level >= 6) {
            world_begin_level(world, 1);
        } else {
            world_advance_level(world);
        }
        return;
    }
}