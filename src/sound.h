#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>
#include <stdint.h>

void sound_init(void);
void sound_update(void);
void sound_play_weapon_fire(uint8_t weapon_type);
void sound_play_enemy_hit(bool killed);
void sound_play_enemy_kill(void);
void sound_play_boss_kill(void);
void sound_play_morse_tone(uint16_t duration_ms);

#endif