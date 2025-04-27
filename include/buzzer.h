#pragma once
#include <stdint.h>

#define PWM_STEPS 45

#define PAUSE 0
#define F_SOUND(f) \
    (uint8_t)(F_CPU / (2UL * PWM_STEPS * (f)))


typedef struct {
    uint8_t frequency;
    uint8_t duration_ms;
} sound_s;

void initialize_buzzer();
void play_sound();
void stop_sound();

void set_sound_frequency(uint8_t frequency);
void set_sound_duration(uint8_t duration);

extern volatile bool sound_finished;

