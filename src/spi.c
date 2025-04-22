/* 
    Module for bit-banged SPI communication between a W5500 and AVR ATtiny85 (or ATmega328P) 
*/

#include "spi.h"

#define LOW(pin) PORTB &= ~_BV(pin)
#define HIGH(pin) PORTB |= _BV(pin)
#define WRITEOUTPUT(out) PORTB = (PORTB & ~_BV(MO)) | (out << MO)
#define READINPUT ((PINB & _BV(MI)) >> MI)

/* Sends header to start off transmission */
void start_transmission(uint32_t addr);
/* Sets chip select, clock signal low to end transmission */
void end_transmission(void);
/* Feeds a byte into the MOSI line bit by bit */
void write_byte(uint8_t data);

uint8_t do_arrays_differ(uint8_t *array_1, uint8_t *array_2, uint8_t array_len) {
    uint8_t diff = 0;
    for (uint8_t i = 0; i < array_len; i++) {
        diff += array_1[i] ^ array_2[i];
    }
    return diff;
}

/*  Reads a 2-byte register value repeatedly until the value matches on two consecutive reads.
    As a 2-byte value has to be read in two pieces, there is a possibility of the value changing mid-read. 
    Returns the read value. */
uint16_t get_2_byte(uint32_t addr) {
    uint8_t read1[] = {0, 0}, read2[] = {0, 0}; 
    do {
        read2[0] = read1[0];
        read2[1] = read1[1];
        read(addr, read1, 2, 2);
    } while (read1[0] != read2[0] || read1[1] != read2[1]);
    return ((uint16_t)read2[0] << 8 | read2[1]);
}

/* Initializes the pins needed for SPI transmissions */
void spi_init() {
    // Set pins to I or O as needed
    DDRB |= _BV(CLK) | _BV(SEL) | _BV(MO);
    DDRB &= ~_BV(MI);
    INTREG &= ~_BV(INTR);

    // Pins to 0, except for the select pin, which defaults to 1 as a low-active
    HIGH(SEL);
    HIGH(CLK);
    LOW(MO);
}

/*  Reads data from a given address into to given buffer. 
    Returns 0 on full read, difference between buffer length and read length if buffer space is insufficient to hold the whole message. */
void read(uint32_t addr, uint8_t *buffer, uint8_t buffer_len, uint8_t read_len) {
    // Send header
    start_transmission(addr);

    // Check read length against available buffer size, cap if necessary
    uint8_t len = MIN(read_len, buffer_len);

    uint8_t byte = 0;
    for (uint16_t i = 0; i < len; i++) {
        // Reads MISO line, fills byte bit by bit
        byte = 0;
        for (int j = 0; j < 8; j++) {
            LOW(CLK);
            byte = (byte << 1) + READINPUT;
            HIGH(CLK);
        }
        buffer[i] = byte;
    }
    end_transmission();
}

void read_reversed(uint32_t addr, uint8_t *buffer, uint8_t buffer_len, uint8_t read_len) {
    // Send header
    start_transmission(addr);

    // Check read length against available buffer size, cap if necessary
    uint8_t len = MIN(read_len, buffer_len);

    uint8_t byte = 0;
    LOW(CLK);
    for (uint8_t i = 0; i < len; i++) {
        // Reads MISO line, fills byte bit by bit
        byte = 0;
        for (int j = 0; j < 8; j++) {
            byte |= (READINPUT << j);
            HIGH(CLK);
            LOW(CLK);
        }
        buffer[i] = byte;
    }
    end_transmission();
}

/* Writes an array to the W5500's registers. */
void write(uint32_t addr, uint16_t data_len, const uint8_t *data) {
    // Set write bit in header frame
    uint32_t address = addr | _BV(2);
    // Send header
    start_transmission(address);

    // Uses write_byte to push message through the pipeline byte by byte
    for (uint16_t i = 0; i < data_len; i++) {
        write_byte(data[i]);
    }

    end_transmission();
}

/* Writes an array stored in progmem to the W5500's registers. */
void write_P(uint32_t addr, uint16_t data_len, const uint8_t *data) {    
    // Set write bit in header frame
    uint32_t address = addr | _BV(2);
    // Send header
    start_transmission(address);

    // Uses write_byte to push message through the pipeline byte by byte
    for (uint16_t i = 0; i < data_len; i++) {
        write_byte(pgm_read_byte(data + i));
    }

    end_transmission();
}

/* Writes a single byte repeatedly to the W5500's registers. */
void write_singular(uint32_t addr, uint16_t data_len, uint8_t data) {    
    // Set write bit in header frame
    uint32_t address = addr | _BV(2);
    // Send header
    start_transmission(address);

    // Uses write_byte to push message through the pipeline byte by byte
    for (uint16_t i = 0; i < data_len; i++) {
        write_byte(data);
    }

    end_transmission();
}


/* Sends header to start off transmission */
void start_transmission(uint32_t addr) {
    // Don't let a writing operation get interrupted by INT0, as that contains other
    // reads and writes that would mess with the chip select and message contents
    DISABLEINT0;

    spi_init();
    
    // Chip select low to initiate transmission
    LOW(SEL);
    
    // HEADER:
    // The first byte of the 4-byte header is irrelevant
    write_byte((uint8_t)(addr >> 16));
    write_byte((uint8_t)(addr >> 8));
    write_byte((uint8_t)addr);

    // Header sent, set output pin to low
    LOW(MO);
}

/* Sets chip select, clock signal low to end transmission */ 
void end_transmission() {
    // Chip select high to end transmission, 
    // clock pin high because it doubles as the UART output, which is low active
    PORTB |= _BV(SEL) | _BV(CLK);

    // Re-enable interrupts
    ENABLEINT0;
}

/* Feeds a byte into the MOSI line bit by bit */
void write_byte(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        LOW(CLK);
        WRITEOUTPUT(EXTRACTBIT(data, i));
        HIGH(CLK);
    }
}

