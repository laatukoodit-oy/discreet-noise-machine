#include "uart.h"
#include <util/delay.h>
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
    SPI.tcp_listen(SPI.sockets[0]);
    
    // Placeholder until interrupts are implemented
    for (;;) {
        _delay_ms(1000);

        // Update status
        SPI.tcp_get_connection_data(&SPI.sockets[0]);

        if (SPI.sockets[0].status == SOCK_CLOSED) {
            SPI.tcp_listen(SPI.sockets[0]);
        }
        // External client connected
        if (SPI.sockets[0].status == SOCK_ESTABLISHED) {
            char msg[] = {"HTTP/1.1 OK\nContent-Type: text/html\n\n<!DOCTYPE html><html><body><h1>Congratulations, you've hacked into the Laatukoodit Oy mainframe.</h1></body></html>"};
            SPI.tcp_send(SPI.sockets[0], sizeof(msg), msg);
            SPI.tcp_disconnect(SPI.sockets[0]);
        }
        // The counterparty sent a connection close request
        if (SPI.sockets[0].status == SOCK_CLOSE_WAIT) {
            SPI.tcp_listen(SPI.sockets[0]);
            // Tähän voisi laittaa homman, joka lukee bufferin viimeiset sisällöt ja sitten
            // sulkee ja uudelleenavaa socketin
        }
    }

    return 0;
}
