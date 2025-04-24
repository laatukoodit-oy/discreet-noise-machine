/* 
    Module for bit-banged SPI communication between a W5500 and AVR ATtiny85 (or ATmega328P) 
*/

#pragma once

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>


// Device-specific pin assignments
#if defined(__AVR_ATtiny85__)
    // External interrupt    
    #define INT_MODE MCUCR
    #define INT_ENABLE GIMSK
    // SPI clock 
    #define CLK PB1
    // SPI chip select
    #define SEL PB4
    // SPI master out
    #define MO PB0
    // SPI master in
    #define MI PB3
    // W5500 interrupt signal
    #define INTR PB2
    #define INTREG DDRB
#elif defined(__AVR_ATmega328P__)
    #define INT_MODE EICRA
    #define INT_ENABLE EIMSK
    // SPI clock 
    #define CLK PB0
    // SPI chip select
    #define SEL PB1
    // SPI master out
    #define MO PB2
    // SPI master in
    #define MI PB3
    // W5500 interrupt signal
    #define INTR PD2
    #define INTREG DDRD
#else 
    #error "Not a supported microcontroller"
    // These are only here to stop the red squiggly lines from undefined macros
    // External interrupt    
    #define INT_MODE MCUCR
    #define INT_ENABLE GIMSK
    // SPI clock 
    #define CLK PB1
    // SPI chip select
    #define SEL PB4
    // SPI master out
    #define MO PB0
    // SPI master in
    #define MI PB3
    // W5500 interrupt signal
    #define INTR PB2
    #define INTREG DDRB
#endif

#define EXTRACTBIT(byte, index) ((byte & (1 << index)) >> index)
// Turns off the INT0 interrupt to prevent a write or read operation from getting interrupted
#define ENABLEINT0 (INT_ENABLE |= (1 << INT0))
#define DISABLEINT0 (INT_ENABLE &= (INT_ENABLE & ~(1 << INT0)))

#define MIN(a, b) ((a < b) ? a : b)

/* Dark arts below, grandparental advisory required */
#define EVAL(...) EVAL8(__VA_ARGS__)
#define EVAL1(...) __VA_ARGS__
#define EVAL2(...) EVAL1(EVAL1(__VA_ARGS__))
#define EVAL4(...) EVAL2(EVAL2(__VA_ARGS__))
#define EVAL8(...) EVAL4(EVAL4(__VA_ARGS__))
#define EMPTY() 
#define DEFER(x) x EMPTY()
#define LOOP(list, i, element, args...) \
    list[i] = element; __VA_OPT__(DEFER(_LOOP)()(list, (i + 1), args))
#define _LOOP() LOOP
#define ASSIGN(list, i, element, args...) \
    EVAL(LOOP(list, i, element, args))

uint8_t do_arrays_differ(uint8_t *array_1, uint8_t *array_2, uint8_t array_len);

/*  Reads a 2-byte register value repeatedly until the value matches on two consecutive reads.
    As a 2-byte value has to be read in two pieces, there is a possibility of the value changing mid-read. 
    Returns the read value. */
uint16_t get_2_byte(uint32_t addr);
/* Initializes the pins needed for SPI transmissions */
void spi_init();

/*  Reads data from a given address into to given buffer. 
    Returns 0 on full read, difference between buffer length and read length if buffer space is insufficient to hold the whole message. */
void read(uint32_t addr, uint8_t *buffer, uint8_t buffer_len, uint8_t read_len);
void read_reversed(uint32_t addr, uint8_t *buffer, uint8_t buffer_len, uint8_t read_len);

/* Writes an array to the W5500's registers. */
void write(uint32_t addr, uint16_t data_len, const uint8_t *data);
/* Writes an array stored in progmem to the W5500's registers. */
void write_P(uint32_t addr, uint16_t data_len, const uint8_t *data);
/* Writes a single byte repeatedly to the W5500's registers. */
void write_singular(uint32_t addr, uint16_t data_len, uint8_t data);