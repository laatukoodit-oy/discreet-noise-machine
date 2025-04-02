#ifndef W5500_SPI_H
#define W5500_SPI_H

#include <stdio.h>
#include <avr/io.h>


/* AVR */
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

// Reads 2-byte value until it stabilises
uint16_t get_2_byte(uint32_t addr);
// Sets up the IO pins used for communication
void spi_init();
// Get data from W5500
void read(uint32_t addr, uint8_t *buffer, uint8_t buffer_len, uint8_t read_len);
// Write data to the W5500
void write(uint32_t addr, uint8_t data_len, uint8_t data[]);
void write_P(uint32_t addr, uint8_t data_len, uint8_t data[]);

#endif