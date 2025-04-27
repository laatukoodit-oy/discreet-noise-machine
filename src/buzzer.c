#include <avr/io.h>
#include <avr/interrupt.h>
#include "buzzer.h"

// Timer 1 overflow interrupt checks the elapsed
// ticks against this variable.
static volatile uint8_t sound_duration = 0;
static volatile bool pause = false;

// Set to true when timer 1 ticks exceed
// the specified `sound_duration`.
// User is responsible for resetting this flag.
volatile bool sound_finished = false;


static inline void setup_timer0() {
    TCCR0A |= _BV(WGM01);
    TIMSK |= _BV(OCIE0A);
}


static inline void setup_timer1() {
    TCCR1 |= _BV(PWM1A);
    OCR1C = PWM_STEPS;
    OCR1A = 0;
}


void initialize_buzzer() {
    setup_timer0();
    setup_timer1();
}


// Sets timer0 and timer1 prescalers to 1
// to start them and connects PWM pin to the timer.
void play_sound() {
    TCCR0B |= _BV(CS00);
    TCCR1 |= _BV(COM1A0);
    TCCR1 |= _BV(CS10);
}


// Sets timer0 and timer1 prescalers to 0
// to stop them and disconnects PWM pin from the timer.
void stop_sound() {
    TCCR0B &= ~_BV(CS00);
    TCCR1 &= ~_BV(COM1A0);
    TCCR1 &= ~_BV(CS10);
}


// Sets the register which determines the timer1 overflow
// interrupt frequency, which adjusts the pulse width.
// Faster pulse width changes => higher frequency.
inline void set_sound_frequency(uint8_t frequency) {
    if (!frequency) {
        pause = true;
        return;
    }

    OCR0A = frequency;
    pause = false;
}


// Sets the variable which the timer1 overflow interrupt
// checks the elapsed ticks against.
inline void set_sound_duration(uint8_t duration) {
    sound_duration = duration;
}


typedef enum: int8_t {
    DOWN = -1,
    UP = 1
} direction_e;


/*
    Unfortunately Fedora ships AVR LibC 2.2.0 while MSYS2 ships version 2.1.0
    and the "Timer/Counter1 Compare Match A" vector changed names between these:

https://avrdudes.github.io/avr-libc/avr-libc-user-manual-2.2.0/group__avr__interrupts.html
https://avrdudes.github.io/avr-libc/avr-libc-user-manual-2.1.0/group__avr__interrupts.html
*/
#if __AVR_LIBC_MINOR__ == 2
ISR(TIMER0_COMPA_vect) {
#else
ISR(TIM0_COMPA_vect) {
#endif
    // `direction` needs to be initialized as DOWN.
    // Otherwise it will break the PWM. Do not touch this.
    static direction_e direction = DOWN;
    static uint8_t timer_ticks[2] = {};
    static uint8_t ms_ticks = 0;

    uint8_t compare_value = OCR0A;

    // First byte of `timer_ticks` hasn't overflowed
    if ((timer_ticks[0] += compare_value) >= compare_value) {
        goto adjust_pulse_width;
    }

    // Less than ~7936 ticks in total have elapsed
    // (0.992ms @ 8MHz)
    if (++timer_ticks[1] < 0x1f) {
        goto adjust_pulse_width;
    }

    timer_ticks[0] = 0;
    timer_ticks[1] = 0;

    // Enough ticks for ~0.992ms elapsed -> increment ms counter
    // and check against duration
    if (++ms_ticks < sound_duration) {
        goto adjust_pulse_width;
    }

    ms_ticks = 0;
    sound_finished = true;

adjust_pulse_width:
    uint8_t current = OCR1A;

    // Change counting direction when register maximum
    // or desired minimum is reacher.
    switch (current) {
        case 0:
            direction = UP;
            break;
        case PWM_STEPS:
            direction = DOWN;
            break;
        default:
            break;
    }

    // Increment/decrement Timer0 compare register B
    // which adjusts the pulse width to generate a
    // triangle wave.
    if (!pause)
        OCR1A += direction;
}

