#pragma once

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

/* SPI */
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
    // These are only here to stop the red squiggly lines
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
#define ENABLEINT0 (INT_ENABLE |= (1 << INT0))
#define DISABLEINT0 (INT_ENABLE &= (INT_ENABLE & ~(1 << INT0)))

/* SPI */
// Reads 2-byte value until it stabilises
uint16_t get_2_byte(uint32_t addr);
// Sets up the IO pins used for communication
void spi_init();

// Get data from W5500
void read(uint32_t addr, uint8_t *buffer, uint8_t buffer_len, uint8_t read_len);
// Write data to the W5500
// The three writes: from array, from progmem, repeats a single character
void write(uint32_t addr, uint16_t data_len, const uint8_t *data);
void write_P(uint32_t addr, uint16_t data_len, const uint8_t *data);
void write_singular(uint32_t addr, uint8_t data_len, uint8_t data);

// Sends header to start off transmission
void start_transmission(uint32_t addr);
// Chip select low
void end_transmission(void);
// 8 cycles of shifting a byte and setting an output port based on said bit
void write_byte(uint8_t data);
