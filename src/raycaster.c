#include "raycaster.h"

#include <stdint.h>
#include <string.h>

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
                        uint8_t *tile_type, int32_t *wall_fraction) {
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
            int64_t hit_coordinate = *hit_vertical
                ? world->player_y + (((int64_t)ray_y * distance) >> 16)
                : world->player_x + (((int64_t)ray_x * distance) >> 16);
            *wall_fraction = (int32_t)(hit_coordinate & 0xffff);
            return distance < FIXED_ONE / 8 ? FIXED_ONE / 8 : distance;
        }
    }
    *hit_vertical = false;
    *tile_type = 'T';
    *wall_fraction = 0;
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

static uint32_t pack_color(int red, int green, int blue, int intensity) {
    return ((uint32_t)((red * intensity) / 255) << 24)
        | ((uint32_t)((green * intensity) / 255) << 16)
        | ((uint32_t)((blue * intensity) / 255) << 8);
}

static uint32_t wall_color(int32_t distance, bool vertical, uint8_t tile_type) {
    if (tile_type == 'X') {
        return 0;
    }
    int intensity = wall_intensity(distance);
    if (tile_type == 'Y') {
        return pack_color(255, 210, 0, intensity < 60 ? 60 : intensity);
    }
    if (tile_type == 'B') {
        return pack_color(0, 70, 255, intensity < 60 ? 60 : intensity);
    }
    if (tile_type == 'W') {
        return pack_color(220, 220, 220, intensity < 60 ? 60 : intensity);
    }
    if (tile_type == 'D') {
        return pack_color(40, 40, 40, intensity < 60 ? 60 : intensity);
    }
    if (tile_type == 'T') {
        return pack_color(255, 60, 150, intensity < 60 ? 60 : intensity);
    }
    if (tile_type == 't') {
        return pack_color(255, 255, 255, 255);
    }
    if (tile_type == 'E') {
        return pack_color(80, 255, 140, 200);
    }
    if (tile_type == 'H') {
        return pack_color(100, 100, 120, intensity < 50 ? 50 : intensity);
    }
    if (tile_type == 'F') {
        return pack_color(25, 110, 30, intensity < 50 ? 50 : intensity);
    }
    if (tile_type >= '1' && tile_type < '1' + BUTTON_COUNT) {
        return pack_color(255, 0, 0, intensity < 60 ? 60 : intensity);
    }
    int red = vertical ? 46 : 88;
    int green = vertical ? 108 : 92;
    int blue = vertical ? 62 : 94;
    return pack_color(red, green, blue, intensity);
}

static uint32_t enemy_color(uint8_t type, bool attacking, int32_t distance) {
    int red = 168;
    int green = 28;
    int blue = 28;
    if (type == ENEMY_TYPE_NORMAL) {
        red = 220;
        green = 110;
        blue = 10;
    } else if (type == ENEMY_TYPE_ARMORED) {
        red = 150;
        green = 30;
        blue = 200;
    }
    int intensity = wall_intensity(distance);
    if (intensity < ENEMY_MIN_INTENSITY) {
        intensity = ENEMY_MIN_INTENSITY;
    }
    if (attacking) {
        red = red + (255 - red) / 3;
        green = green + (255 - green) / 3;
        blue = blue + (255 - blue) / 3;
        intensity = 140;
    }
    return pack_color(red, green, blue, intensity);
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

        int center_column = DISPLAY_WIDTH / 2
            + (int)(((int64_t)side * ENEMY_SCREEN_SCALE) / forward);
        int sprite_height = (int)(((int64_t)WALL_PROJECTION_DISTANCE) / forward);
        if (sprite_height < 1) {
            sprite_height = 1;
        }
        if (sprite_height > DISPLAY_HEIGHT) {
            sprite_height = DISPLAY_HEIGHT;
        }
        int sprite_width = sprite_height / 2;
        if (sprite_width < 1) {
            sprite_width = 1;
        }
        int top_row = (DISPLAY_HEIGHT - sprite_height) / 2;
        int bottom_row = top_row + sprite_height;
        int left_column = center_column - sprite_width / 2;
        int right_column = left_column + sprite_width;
        uint32_t color = enemy_color(enemy->type, enemy->attack_frames > 0, forward);

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

// Bottom-row dot at the target's bearing; clamped to the edge when it is off-screen or behind.
static void render_target_dot(const World *world) {
    int target = world->button_target;
    int32_t delta_x = world->button_x[target] - world->player_x;
    int32_t delta_y = world->button_y[target] - world->player_y;
    int32_t forward = (int32_t)(((int64_t)delta_x * world_cos(world->angle)
        + (int64_t)delta_y * world_sin(world->angle)) >> 15);
    int32_t side = (int32_t)((-(int64_t)delta_x * world_sin(world->angle)
        + (int64_t)delta_y * world_cos(world->angle)) >> 15);
    int column;

    if (forward > FIXED_ONE / 8) {
        column = DISPLAY_WIDTH / 2 + (int)(((int64_t)side * ENEMY_SCREEN_SCALE) / forward);
    } else {
        column = side >= 0 ? DISPLAY_WIDTH - 1 : 0;
    }
    if (column < 0) {
        column = 0;
    }
    if (column > DISPLAY_WIDTH - 1) {
        column = DISPLAY_WIDTH - 1;
    }
    display_set_pixel(column, DISPLAY_HEIGHT - 1, 0xff000000u);
}

// Classic 5x7 font columns, bit 0 = top row. Only the characters used by INTRO_MESSAGE.
static uint8_t font_column(char character, int column) {
    static const uint8_t glyphs[][5] = {
        {0x00, 0x05, 0x03, 0x00, 0x00},  // '
        {0x27, 0x45, 0x45, 0x45, 0x39},  // 5
        {0x7E, 0x11, 0x11, 0x11, 0x7E},  // A
        {0x7F, 0x49, 0x49, 0x49, 0x36},  // B
        {0x3E, 0x41, 0x41, 0x41, 0x22},  // C
        {0x7F, 0x41, 0x41, 0x22, 0x1C},  // D
        {0x7F, 0x49, 0x49, 0x49, 0x41},  // E
        {0x3E, 0x41, 0x49, 0x49, 0x7A},  // G
        {0x20, 0x40, 0x41, 0x3F, 0x01},  // J
        {0x7F, 0x04, 0x08, 0x10, 0x7F},  // N
        {0x3E, 0x41, 0x41, 0x41, 0x3E},  // O
        {0x7F, 0x09, 0x09, 0x09, 0x06},  // P
        {0x7F, 0x09, 0x19, 0x29, 0x46},  // R
        {0x46, 0x49, 0x49, 0x49, 0x31},  // S
        {0x01, 0x01, 0x7F, 0x01, 0x01},  // T
        {0x3F, 0x40, 0x40, 0x40, 0x3F},  // U
        {0x1F, 0x20, 0x40, 0x20, 0x1F},  // V
        {0x07, 0x08, 0x70, 0x08, 0x07},  // Y
    };
    static const char order[] = "'5ABCDEGJNOPRSTUVY";

    for (int index = 0; order[index] != '\0'; ++index) {
        if (order[index] == character) {
            return glyphs[index][column];
        }
    }
    return 0;
}

static void render_text(const char *message, int scroll) {
    int text_columns = (int)strlen(message) * 6;

    for (int x = 0; x < DISPLAY_WIDTH; ++x) {
        int text_column = scroll + x - DISPLAY_WIDTH;
        uint8_t bits = 0;
        if (text_column >= 0 && text_column < text_columns && text_column % 6 < 5) {
            bits = font_column(message[text_column / 6], text_column % 6);
        }
        for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
            display_set_pixel(x, y, (bits >> y) & 1u ? 0xffa00000u : 0);
        }
    }
}

static void render_intro(const World *world) {
    render_text(INTRO_MESSAGE, (INTRO_FRAMES - world->intro_frames) / 2);
}

// Night view of the facility from outside; the camera slowly pulls back while the player walks off.
static void render_exterior(int time) {
    static const uint8_t stars[][2] = {{2, 0}, {5, 2}, {7, 1}, {12, 0}, {14, 2}, {10, 1}};
    int width = 12 - 2 * (time * 3 / ENDING_VIEW_FRAMES);
    int height = time < ENDING_VIEW_FRAMES / 2 ? 4 : 3;
    int left = (DISPLAY_WIDTH - width) / 2;
    int top = 5 - height;

    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        for (int x = 0; x < DISPLAY_WIDTH; ++x) {
            display_set_pixel(x, y, y < 5 ? pack_color(0, 0, 40, 255) : pack_color(0, 30, 0, 255));
        }
    }
    for (int index = 0; index < 6; ++index) {
        display_set_pixel(stars[index][0], stars[index][1], pack_color(120, 120, 140, 255));
    }
    for (int y = top; y < 5; ++y) {
        for (int x = left; x < left + width; ++x) {
            display_set_pixel(x, y, y == top ? pack_color(60, 60, 70, 255) : pack_color(90, 90, 100, 255));
        }
    }
    for (int x = left + 1; x < left + width - 1; x += 2) {
        if (x < 6 || x > 9) {
            bool lit = ((time / 20) + x) % 3 != 0;
            display_set_pixel(x, top + 1, lit ? pack_color(255, 200, 50, 255) : pack_color(60, 45, 10, 255));
        }
    }
    for (int y = 3; y < 5; ++y) {
        display_set_pixel(7, y, pack_color(80, 255, 140, 255));
        display_set_pixel(8, y, pack_color(80, 255, 140, 255));
    }
    int figure_x = 8 + time * 6 / ENDING_VIEW_FRAMES;
    display_set_pixel(figure_x, 5, pack_color(255, 255, 255, 255));
    display_set_pixel(figure_x, 6, pack_color(200, 200, 200, 255));
}

static void render_ending(const World *world) {
    int elapsed = ENDING_FRAMES - world->ending_frames;

    if (elapsed < ENDING_TEXT_FRAMES) {
        render_text(ENDING_MESSAGE, elapsed / 2);
    } else {
        render_exterior(elapsed - ENDING_TEXT_FRAMES);
    }
}

void raycaster_render(const World *world) {
    int escape_elapsed = ESCAPE_RUN_FRAMES + ESCAPE_FLASH_FRAMES - world->escape_frames;
    if (world->escape_frames > 0 && escape_elapsed >= ESCAPE_RUN_FRAMES) {
        int level = (escape_elapsed - ESCAPE_RUN_FRAMES + 1) * 255 / ESCAPE_FLASH_FRAMES;
        uint32_t white = ((uint32_t)level << 24) | ((uint32_t)level << 16) | ((uint32_t)level << 8);
        for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
            for (int x = 0; x < DISPLAY_WIDTH; ++x) {
                display_set_pixel(x, y, white);
            }
        }
        return;
    }
    if (world->intro_frames > 0) {
        render_intro(world);
        return;
    }
    if (world->ending_frames > 0) {
        render_ending(world);
        return;
    }

    int32_t wall_distances[DISPLAY_WIDTH];

    for (int column = 0; column < DISPLAY_WIDTH; ++column) {
        bool hit_vertical;
        uint8_t tile_type;
        int32_t wall_fraction;
        uint8_t ray_angle = (uint8_t)(world->angle + ray_offsets[column]);
        int32_t distance = cast_ray(world, ray_angle, &hit_vertical, &tile_type, &wall_fraction);
        int32_t perpendicular_distance = (int32_t)(((int64_t)distance
            * world_cos((uint8_t)ray_offsets[column])) >> 15);
        if (tile_type >= '1' && tile_type < '1' + BUTTON_COUNT
            && world->button_pressed[tile_type - '1']) {
            tile_type = 'D';
        }
        if (tile_type == 'T' && world->test_button_frames > 0) {
            tile_type = 't';
        }
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

        int wall_top = (DISPLAY_HEIGHT - wall_height) / 2 + world->view_bob;
        int wall_bottom = wall_top + wall_height;
        uint32_t wall;
        uint32_t patch = 0;
        bool button_face = (tile_type >= '1' && tile_type < '1' + BUTTON_COUNT)
            || tile_type == 'D' || tile_type == 'T' || tile_type == 't';
        int patch_top = wall_top;
        int patch_bottom = wall_bottom;
        if (button_face) {
            // Buttons are a small square in the middle of an ordinary wall tile.
            wall = wall_color(perpendicular_distance, hit_vertical, '#');
            if (wall_fraction >= 0x4000 && wall_fraction < 0xc000) {
                patch = wall_color(perpendicular_distance, hit_vertical, tile_type);
                if (wall_height >= 4) {
                    patch_top = wall_top + wall_height / 4;
                    patch_bottom = wall_bottom - wall_height / 4;
                }
            }
        } else {
            wall = wall_color(perpendicular_distance, hit_vertical, tile_type);
        }
        for (int row = 0; row < DISPLAY_HEIGHT; ++row) {
            uint32_t color = row < wall_top
                ? (world->outside ? pack_color(0, 0, 40, 255) : 0x00000200u)
                : (world->outside ? pack_color(0, 30, 0, 255) : 0);
            if (row >= wall_top && row < wall_bottom) {
                color = (patch != 0 && row >= patch_top && row < patch_bottom) ? patch : wall;
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
        uint32_t top_color = world->weapon_type == WEAPON_SHOTGUN ? 0xff400000u : 0x0000a0ffu;
        uint32_t bottom_color = world->weapon_type == WEAPON_SHOTGUN ? 0xffff0000u : 0x0000ffffu;
        display_set_pixel(7, 3, top_color);
        display_set_pixel(8, 3, top_color);
        display_set_pixel(7, 4, bottom_color);
        display_set_pixel(8, 4, bottom_color);
    }

    uint8_t remaining_hits = world->player_hits >= 10 ? 0 : (10 - world->player_hits);
    uint32_t red_intensity = ((uint32_t)remaining_hits * 255u) / 10u;
    display_set_pixel(DISPLAY_WIDTH - 1, 0,
        (red_intensity << 24));
    if (world->escape_frames == 0 && !world->outside) {
        render_target_dot(world);
    }
    for (int index = 0; index < BUTTON_COUNT; ++index) {
        if (!world->button_pressed[index]) {
            display_set_pixel(index, 0, 0xa0000000u);
        }
    }
}