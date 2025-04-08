#include "uart.h"

void uart_init() {
    // Set PB1 as output.
    SET_UART_PIN;
    // Idle high (UART inactive state).
    SET_UART_INACTIVE;
}


// Did not come up with this myself...
static inline uint8_t reverse_byte(uint8_t x) {
    // Reverses bits next to each other.
    x = ((x >> 1) & 0x55) | ((x << 1) & 0xaa);
    // Reverses chunks of 2 bits next to each other.
    x = ((x >> 2) & 0x33) | ((x << 2) & 0xcc);
    // And finally reverses the 2 halves of the byte.
    x = ((x >> 4) & 0x0f) | ((x << 4) & 0xf0);
    return x;
}


void uart_putchar(char byte) {
    // Define baud rate and signal duration
    // in microseconds for each bit.
    constexpr uint16_t baud_rate = 9600;
    constexpr uint8_t bit_delay_us = 1'000'000 / baud_rate;

    // Disable interrupts to maintain UART timing.
    uint8_t interrupts = SREG;
    SREG &= ~(1 << SREG_I);

    SET_UART_INACTIVE;
    

    // Pull down PB1 (DO/TX) to signal start of a transmission.
    PORTB &= ~_BV(PB1);
    // Wait for one bit time (104us at 9600 baud).
    _delay_us(bit_delay_us);

    // Set data register.
    // UART expects LSB first so we need to reverse
    // the byte.
    USIDR = reverse_byte(byte);

    // Enable USI three wire mode.
    // This latches the MSB of USIDR
    // onto DO/TX (PB1) immediately.
    // So this sends the first bit.
    USICR |= _BV(USIWM0);

    // Wait for bit delay.
    _delay_us(bit_delay_us);

    // Send the remaining 7 data bits.
    for (uint8_t i = 0; i < 7; i++) {
        // Strobe clock bit
        USICR |= _BV(USICLK);
        _delay_us(bit_delay_us);
    }

    // Clear USI control register.
    // This allows PB1 to be controlled by the port again.
    USICR &= ~_BV(USIWM0);

    // Pull PB1 (DO/TX) high at the end of the transmission.
    SET_UART_INACTIVE;
    _delay_us(bit_delay_us);

    // Re-enable interrupts.
    SREG |= interrupts;
}


void uart_write(const char *data) {
    char byte;
    // Write characters until terminating null byte
    while ((byte = *data++))
        uart_putchar(byte);
}


// Write characters of a string stored in flash memory
void uart_write_P(const char* data) {
    char byte;
    while ((byte = pgm_read_byte(data++)))
        uart_putchar(byte);
}


#ifndef __AVR_ATtiny85__
void print_buffer(const uint8_t *buffer, uint8_t buffer_len, uint8_t print_len) {
    
}
#endif
#ifdef __AVR_ATtiny85__
void print_buffer(const uint8_t *buffer, uint8_t buffer_len, uint8_t print_len) {
    uint8_t len = (print_len <= buffer_len ? print_len : buffer_len);
    uint8_t byte;
    for (uint8_t i = 0; i < len; i++) {  
        byte = buffer[i];
        uart_putchar(byte);
    }
}
#endif