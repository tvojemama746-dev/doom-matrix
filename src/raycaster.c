#include "raycaster.h"

#include <stdint.h>

#include "display_ws2812.h"

#define FAR_DISTANCE (32 * FIXED_ONE)
#define WALL_PROJECTION_DISTANCE (14 * FIXED_ONE)
#define MIN_LIGHT_INTENSITY 2
#define MAX_WALL_INTENSITY 160
#define LIGHT_FALLOFF_NUMERATOR (64ull * FIXED_ONE * FIXED_ONE)
#define ENEMY_SCREEN_SCALE 20
#define ENEMY_MIN_INTENSITY 48

static const int8_t ray_offsets[DISPLAY_WIDTH] = {
    -22, -19, -16, -13, -10, -7, -4, -1,
      1,   4,   7,  10,  13, 16, 19, 22
};

static int32_t ray_delta_distance(int32_t ray_component) {
    if (ray_component == 0) {
        return FAR_DISTANCE;
    }
    int32_t absolute_component = ray_component < 0 ? -ray_component : ray_component;
    return (int32_t)(((int64_t)FIXED_ONE * FIXED_ONE) / absolute_component);
}

static int32_t cast_ray(const World *world, uint8_t ray_angle, bool *hit_vertical,
                        uint8_t *tile_type) {
    int32_t ray_x = (int32_t)world_cos(ray_angle) << 1;
    int32_t ray_y = (int32_t)world_sin(ray_angle) << 1;
    int map_x = world->player_x >> 16;
    int map_y = world->player_y >> 16;
    int step_x = ray_x < 0 ? -1 : 1;
    int step_y = ray_y < 0 ? -1 : 1;
    int32_t delta_x = ray_delta_distance(ray_x);
    int32_t delta_y = ray_delta_distance(ray_y);
    int32_t side_x;
    int32_t side_y;

    if (ray_x < 0) {
        side_x = (int32_t)(((int64_t)(world->player_x - (map_x << 16)) * delta_x) >> 16);
    } else {
        side_x = (int32_t)(((int64_t)(((map_x + 1) << 16) - world->player_x) * delta_x) >> 16);
    }
    if (ray_y < 0) {
        side_y = (int32_t)(((int64_t)(world->player_y - (map_y << 16)) * delta_y) >> 16);
    } else {
        side_y = (int32_t)(((int64_t)(((map_y + 1) << 16) - world->player_y) * delta_y) >> 16);
    }

    for (int steps = 0; steps < 32; ++steps) {
        if (side_x < side_y) {
            side_x += delta_x;
            map_x += step_x;
            *hit_vertical = true;
        } else {
            side_y += delta_y;
            map_y += step_y;
            *hit_vertical = false;
        }
        if (world_is_wall(map_x, map_y)) {
            int32_t distance = *hit_vertical ? side_x - delta_x : side_y - delta_y;
            *tile_type = world_tile_type(map_x, map_y);
            return distance < FIXED_ONE / 8 ? FIXED_ONE / 8 : distance;
        }
    }
    *hit_vertical = false;
    *tile_type = 'T';
    return FAR_DISTANCE;
}

static int wall_intensity(int32_t distance) {
    uint64_t distance_squared = (uint64_t)distance * distance;
    int intensity = (int)(LIGHT_FALLOFF_NUMERATOR / distance_squared);

    if (intensity < MIN_LIGHT_INTENSITY) {
        return MIN_LIGHT_INTENSITY;
    }
    if (intensity > MAX_WALL_INTENSITY) {
        return MAX_WALL_INTENSITY;
    }
    return intensity;
}

static uint32_t wall_color(int32_t distance, bool vertical, uint8_t tile_type) {
    if (tile_type == 'X') {
        return 0;
    }
    int red = vertical ? 46 : 88;
    int green = vertical ? 108 : 92;
    int blue = vertical ? 62 : 94;
    int intensity = wall_intensity(distance);
    red = (red * intensity) / 255;
    green = (green * intensity) / 255;
    blue = (blue * intensity) / 255;
    return ((uint32_t)red << 24)
        | ((uint32_t)green << 16)
        | ((uint32_t)blue << 8);
}

static uint32_t enemy_color(bool attacking, int32_t distance) {
    int red = 168;
    int green = 28;
    int blue = 28;
    int intensity = wall_intensity(distance);
    if (intensity < ENEMY_MIN_INTENSITY) {
        intensity = ENEMY_MIN_INTENSITY;
    }
    if (attacking) {
        red = 208;
        green = 40;
        blue = 24;
        intensity = 110;
    }
    return ((uint32_t)((red * intensity) / 255) << 24)
        | ((uint32_t)((green * intensity) / 255) << 16)
        | ((uint32_t)((blue * intensity) / 255) << 8);
}

static bool enemy_is_boss(const World *world, int index) {
    return world->level >= 6 && index == 0;
}

static void render_level_indicator(const World *world) {
    uint8_t level_pixels = world->level;
    if (level_pixels > DISPLAY_WIDTH - 1) {
        level_pixels = DISPLAY_WIDTH - 1;
    }
    for (int x = 0; x < level_pixels; ++x) {
        display_set_pixel(x, 0, 0x00ff0000u);
    }
}

static void render_pause_button(int button_index, bool selected) {
    static const uint8_t x_starts[3] = {0, 5, 10};
    int column = button_index % 3;
    int row = button_index / 3;
    int x0 = x_starts[column];
    int x1 = (column == 2) ? DISPLAY_WIDTH : x_starts[column + 1];
    int y0 = row * 4;
    int y1 = y0 + 4;
    uint32_t color = selected ? 0xff500000u : 0x50300000u;

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            display_set_pixel(x, y, color);
        }
    }

    for (int x = x0; x < x1; ++x) {
        display_set_pixel(x, y0, selected ? 0xffff6000u : 0x80500000u);
    }
    for (int y = y0; y < y1; ++y) {
        display_set_pixel(x0, y, selected ? 0xffff6000u : 0x80500000u);
    }
}

static void render_pause_menu(const World *world) {
    for (int x = 0; x < DISPLAY_WIDTH; ++x) {
        for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
            display_set_pixel(x, y, 0x10100000u);
        }
    }

    for (int button = 0; button < 6; ++button) {
        render_pause_button(button, world->startup_menu_selection == button);
    }
}

static void render_weapon_button(int x0, int x1, bool selected, uint32_t color) {
    for (int y = 1; y < DISPLAY_HEIGHT - 1; ++y) {
        for (int x = x0; x < x1; ++x) {
            display_set_pixel(x, y, selected ? color : (color & 0x7f7f7f00u));
        }
    }
    for (int x = x0; x < x1; ++x) {
        display_set_pixel(x, 1, selected ? 0xffffffffu : 0x80808000u);
        display_set_pixel(x, DISPLAY_HEIGHT - 2, selected ? 0xffffffffu : 0x80808000u);
    }
    for (int y = 1; y < DISPLAY_HEIGHT - 1; ++y) {
        display_set_pixel(x0, y, selected ? 0xffffffffu : 0x80808000u);
        display_set_pixel(x1 - 1, y, selected ? 0xffffffffu : 0x80808000u);
    }
}

static void render_weapon_menu(const World *world) {
    for (int x = 0; x < DISPLAY_WIDTH; ++x) {
        for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
            display_set_pixel(x, y, 0x10100000u);
        }
    }

    render_weapon_button(1, 8, world->weapon_menu_selection == 0, 0x00ff0000u);
    render_weapon_button(8, 15, world->weapon_menu_selection == 1, 0x0000ff00u);
}

static void render_enemy(const World *world, const int32_t wall_distances[DISPLAY_WIDTH]) {
    for (int index = 0; index < WORLD_ENEMY_COUNT; ++index) {
        const Enemy *enemy = &world->enemies[index];
        if (!enemy->alive) {
            continue;
        }

        int32_t delta_x = enemy->x - world->player_x;
        int32_t delta_y = enemy->y - world->player_y;
        int32_t forward = (int32_t)(((int64_t)delta_x * world_cos(world->angle)
            + (int64_t)delta_y * world_sin(world->angle)) >> 15);
        int32_t side = (int32_t)((-(int64_t)delta_x * world_sin(world->angle)
            + (int64_t)delta_y * world_cos(world->angle)) >> 15);
        if (forward <= FIXED_ONE / 8) {
            continue;
        }

        bool boss = enemy_is_boss(world, index);
        int center_column = DISPLAY_WIDTH / 2
            + (int)(((int64_t)side * ENEMY_SCREEN_SCALE) / forward);
        int sprite_height = (int)(((int64_t)WALL_PROJECTION_DISTANCE) / forward);
        if (boss) {
            sprite_height += sprite_height / 2;
        }
        if (sprite_height < 1) {
            sprite_height = 1;
        }
        if (sprite_height > DISPLAY_HEIGHT) {
            sprite_height = DISPLAY_HEIGHT;
        }
        int sprite_width = boss ? sprite_height : sprite_height / 2;
        if (sprite_width < 1) {
            sprite_width = 1;
        }
        int top_row = (DISPLAY_HEIGHT - sprite_height) / 2;
        int bottom_row = top_row + sprite_height;
        int left_column = center_column - sprite_width / 2;
        int right_column = left_column + sprite_width;
        uint32_t color = boss
            ? (enemy->attack_frames > 0 ? 0xffff3000u : 0xffdd0000u)
            : enemy_color(enemy->attack_frames > 0, forward);

        for (int column = left_column; column < right_column; ++column) {
            if (column < 0 || column >= DISPLAY_WIDTH || forward >= wall_distances[column]) {
                continue;
            }
            for (int row = top_row; row < bottom_row; ++row) {
                display_set_pixel(column, row, color);
            }
        }
    }
}

void raycaster_render(const World *world) {
    if (world->weapon_menu) {
        render_weapon_menu(world);
        return;
    }
    if (world->in_menu) {
        render_pause_menu(world);
        return;
    }

    int32_t wall_distances[DISPLAY_WIDTH];

    for (int column = 0; column < DISPLAY_WIDTH; ++column) {
        bool hit_vertical;
        uint8_t tile_type;
        uint8_t ray_angle = (uint8_t)(world->angle + ray_offsets[column]);
        int32_t distance = cast_ray(world, ray_angle, &hit_vertical, &tile_type);
        int32_t perpendicular_distance = (int32_t)(((int64_t)distance
            * world_cos((uint8_t)ray_offsets[column])) >> 15);
        wall_distances[column] = perpendicular_distance;
        int wall_height = (int)(((int64_t)WALL_PROJECTION_DISTANCE)
            / perpendicular_distance);
        if (wall_height > DISPLAY_HEIGHT) {
            wall_height = DISPLAY_HEIGHT;
        }
        if (perpendicular_distance >= FIXED_ONE && wall_height == DISPLAY_HEIGHT) {
            wall_height = DISPLAY_HEIGHT - 1;
        }
        if (wall_height < 1) {
            wall_height = 1;
        }

        int wall_top = (DISPLAY_HEIGHT - wall_height) / 2;
        int wall_bottom = wall_top + wall_height;
        uint32_t wall = wall_color(perpendicular_distance, hit_vertical, tile_type);
        for (int row = 0; row < DISPLAY_HEIGHT; ++row) {
            uint32_t color = row < wall_top ? 0x00000200u : 0;
            if (row >= wall_top && row < wall_bottom) {
                color = wall;
            }
            display_set_pixel(column, row, color);
        }
    }

    render_enemy(world, wall_distances);

    if (world->player_hit_frames > 0) {
        display_set_pixel(0, 3, 0x30030000u);
        display_set_pixel(0, 4, 0x30030000u);
        display_set_pixel(15, 3, 0x30030000u);
        display_set_pixel(15, 4, 0x30030000u);
    }

    if (world->fire_frames > 0) {
        uint32_t top_color = world->weapon_type == 1 ? 0xff400000u : 0x0000a0ffu;
        uint32_t bottom_color = world->weapon_type == 1 ? 0xffff0000u : 0x0000ffffu;
        display_set_pixel(7, 3, top_color);
        display_set_pixel(8, 3, top_color);
        display_set_pixel(7, 4, bottom_color);
        display_set_pixel(8, 4, bottom_color);
    }

    uint8_t remaining_hits = world->player_hits >= 10 ? 0 : (10 - world->player_hits);
    uint32_t red_intensity = ((uint32_t)remaining_hits * 255u) / 10u;
    display_set_pixel(DISPLAY_WIDTH - 1, 0,
        (red_intensity << 24));
    render_level_indicator(world);
}