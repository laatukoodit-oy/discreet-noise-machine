#include <avr/io.h>
#include <stdio.h>

void uart_init(void) {
    UBRR0 = 103; // 9600 baud for 16MHz
    UCSR0B = (1 << TXEN0); // Enable TX
}


// .put field of FILE expects this signature
// so the FILE parameter has to be there
int uart_putchar(char c, [[maybe_unused]] FILE *stream) {
    while (!(UCSR0A & (1 << UDRE0))); // Wait for buffer to be empty
    UDR0 = c;
    return 0;
}


FILE uart_output = {
    .put = uart_putchar,
    .get = NULL,
    .flags = _FDEV_SETUP_WRITE
};


