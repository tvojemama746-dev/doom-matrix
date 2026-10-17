#include "sound.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#define SOUND_PIN 5

typedef struct {
    uint16_t frequency_hz;
    uint16_t duration_ms;
} SoundSegment;

static uint sound_slice;
static bool sound_active;
static absolute_time_t sound_stop_time;
static const SoundSegment *sound_segments;
static uint8_t sound_segment_count;
static uint8_t sound_segment_index;

static const SoundSegment enemy_kill_segments[] = {
    {560, 45},
    {360, 45},
    {220, 90},
};

static const SoundSegment boss_kill_segments[] = {
    {280, 70},
    {180, 110},
    {140, 160},
};

static void sound_start_segment(uint16_t frequency_hz, uint16_t duration_ms) {
    if (frequency_hz == 0) {
        return;
    }

    uint32_t clock_hz = clock_get_hz(clk_sys);
    float divider = (float)clock_hz / ((float)frequency_hz * 256.0f);
    if (divider < 1.0f) {
        divider = 1.0f;
    }

    pwm_set_enabled(sound_slice, false);
    pwm_set_clkdiv(sound_slice, divider);
    pwm_set_wrap(sound_slice, 255);
    pwm_set_gpio_level(SOUND_PIN, 128);
    pwm_set_enabled(sound_slice, true);

    sound_stop_time = delayed_by_ms(get_absolute_time(), duration_ms);
    sound_active = true;
}

static void sound_start_sequence(const SoundSegment *segments, uint8_t segment_count) {
    if (segment_count == 0) {
        return;
    }

    sound_segments = segments;
    sound_segment_count = segment_count;
    sound_segment_index = 0;
    sound_active = true;
    sound_start_segment(sound_segments[0].frequency_hz, sound_segments[0].duration_ms);
}

void sound_init(void) {
    gpio_set_function(SOUND_PIN, GPIO_FUNC_PWM);
    sound_slice = pwm_gpio_to_slice_num(SOUND_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255);
    pwm_init(sound_slice, &config, false);
    pwm_set_gpio_level(SOUND_PIN, 0);
    pwm_set_enabled(sound_slice, false);
    sound_active = false;
}

void sound_update(void) {
    if (!sound_active) {
        return;
    }
    if (!time_reached(sound_stop_time)) {
        return;
    }

    ++sound_segment_index;
    if (sound_segment_index < sound_segment_count) {
        sound_start_segment(sound_segments[sound_segment_index].frequency_hz,
                            sound_segments[sound_segment_index].duration_ms);
        return;
    }

    pwm_set_enabled(sound_slice, false);
    pwm_set_gpio_level(SOUND_PIN, 0);
    sound_active = false;
}

void sound_play_weapon_fire(uint8_t weapon_type) {
    if (weapon_type == 1) {
        static const SoundSegment bazooka_segments[] = {
            {160, 55},
            {110, 90},
        };
        sound_start_sequence(bazooka_segments, 2);
    } else {
        static const SoundSegment rifle_segments[] = {
            {1400, 18},
            {1000, 18},
            {1600, 14},
        };
        sound_start_sequence(rifle_segments, 3);
    }
}

void sound_play_enemy_hit(bool killed) {
    if (killed) {
        sound_start_sequence(enemy_kill_segments, 3);
    } else {
        static const SoundSegment hit_segments[] = {
            {720, 20},
            {540, 20},
        };
        sound_start_sequence(hit_segments, 2);
    }
}

void sound_play_enemy_kill(void) {
    sound_start_sequence(enemy_kill_segments, 3);
}

void sound_play_boss_kill(void) {
    sound_start_sequence(boss_kill_segments, 3);
}

// One short pitched blip per letter; the pitch follows the letter so the text sounds like babble.
void sound_play_speech_blip(char letter) {
    static SoundSegment blip;

    blip.frequency_hz = (letter >= 'A' && letter <= 'Z') ? 350 + (letter - 'A') * 40 : 700;
    blip.duration_ms = 55;
    sound_start_sequence(&blip, 1);
}