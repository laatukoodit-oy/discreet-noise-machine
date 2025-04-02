#include "w5500_spi.h"


#define LOW(pin) PORTB &= ~(1 << pin)
#define HIGH(pin) PORTB |= (1 << pin)
#define WRITEOUTPUT(out) PORTB = (PORTB & ~(1 << MO)) | (out << MO)
#define READINPUT ((PINB & (1 << MI)) >> MI)


// Sends header to start off transmission
void start_transmission(uint32_t addr);
// Chip select low
void end_transmission(void);
// 8 cycles of shifting a byte and setting an output port based on said bit
void writeByte(uint8_t data);


// As 2-byte register values may change during reading, check them until subsequent reads match
uint16_t get_2_byte(uint32_t addr) {
    uint8_t read1[] = {0, 0}, read2[] = {0, 0}; 
    do {
        read2[0] = read1[0];
        read2[1] = read1[1];
        read(addr, read1, 2, 2);
    } while (read1[0] != read2[0] || read1[1] != read2[1]);
    return ((uint16_t)read2[0] << 8 | read2[1]);
}

// Initializes the pins needed for SPI transmissions
void spi_init() {
    // Set pins to I or O as needed
    DDRB |= (1 << CLK) | (1 << SEL) | (1 << MO);
    DDRB &= ~(1 << MI);
    INTREG &= ~(1 << INTR);

    // Pins to 0, except for the select pin, which defaults to 1 as a low-active
    HIGH(SEL);
    HIGH(CLK);
    LOW(MO);
}

// Conducts a read operation fetching information from register(s), number of fetched bytes given by readlen
void read(uint32_t addr, uint8_t *buffer, uint8_t buffer_len, uint8_t read_len) {
    // Don't let a reading operation get interrupted by INT0, as that contains other
    // reads and writes that would mess with the chip select and message contents
    cli();

    //uart_write_P(PSTR("Read.\r\n"));

    spi_init();
    
    // Send header
    start_transmission(addr);

    uint8_t byte = 0;

    // Check read length against available buffer size, cap if necessary
    uint8_t len = (read_len <= buffer_len ? read_len : buffer_len);
    // (could use some sort of error if not all can be read)
    
    for (uint8_t i = 0; i < len; i++) {
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

    sei();
}

// Write to W5500's register(s), number of bytes written given by datalen (should be sizeof(data))
void write(uint32_t addr, uint8_t data_len, uint8_t data[]) {
    // Don't let a writing operation get interrupted by INT0, as that contains other
    // reads and writes that would mess with the chip select and message contents
    cli();

    spi_init();
    
    // Set write bit in header frame
    uint32_t address = addr | (1 << 2);
    // Send header
    start_transmission(address);

    // Uses writeByte to push message through the pipeline byte by byte
    for (uint8_t i = 0; i < data_len; i++) {
        writeByte(data[i]);
    }

    end_transmission();

    sei();
}

// Write to W5500's register(s), number of bytes written given by datalen (should be sizeof(data))
void write_P(uint32_t addr, uint8_t data_len, uint8_t data[]) {
    // Don't let a writing operation get interrupted by INT0, as that contains other
    // reads and writes that would mess with the chip select and message contents
    cli();

    spi_init();
    
    // Set write bit in header frame
    uint32_t address = addr | (1 << 2);
    // Send header
    start_transmission(address);

    // Uses writeByte to push message through the pipeline byte by byte
    for (uint8_t i = 0; i < data_len; i++) {
        writeByte(pgm_read_byte(data + i));
    }

    end_transmission();

    sei();
}

void start_transmission(uint32_t addr) {
    // Chip select low to initiate transmission
    LOW(SEL);
    
    // HEADER:
    // The first byte of the 4-byte header is irrelevant
    writeByte((uint8_t)(addr >> 16));
    writeByte((uint8_t)(addr >> 8));
    writeByte((uint8_t)addr);

    // Header sent, set output pin to low
    LOW(MO);
}

void end_transmission() {
    // Chip select high to end transmission, 
    // clock pin high because it doubles as the UART output, which is low active
    PORTB |= (1 << SEL) | (1 << CLK);
}

// Shift bits from MSB to LSB to the MOSI pipe, cycle clock to send
void writeByte(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        LOW(CLK);
        WRITEOUTPUT(EXTRACTBIT(data, i));
        HIGH(CLK);
    }
}