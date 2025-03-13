#include "uart.h"
#include "w5500_spi.h"

// Buffer for data read from the W5500
#define SPIBUFSIZE 100
uint8_t spi_buffer[SPIBUFSIZE] = {0,};

int main(void) {
    uart_init();
    stdout = &uart_output; // Redirect stdout to UART

    // IP address & other setup
    W5500_SPI SPI = setup_w5500_spi(spi_buffer, SPIBUFSIZE);
    // Opens socket 0 to a TCP listening state on port 9999
    SPI.tcp_listen(&SPI, SPI.sockets[0]);

    return 0;
}
