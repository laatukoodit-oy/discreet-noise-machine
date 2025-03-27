#include <avr/pgmspace.h>
#include "uart.h"
#include <util/delay.h>
#include "w5500.h"
#include <stdlib.h>

// Buffer for data read from the W5500
#define SPIBUFSIZE 100
uint8_t spi_buffer[SPIBUFSIZE] = {0,};

const char index_page[] PROGMEM = "HTTP/1.1 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><body><h1>Test</h1></body></html>";

W5500_SPI W5500;

void interrupt(int socketno, uint8_t interrupt);

int main(void) {
    //uart_init();
    //stdout = &uart_output; // Redirect stdout to UART

    // IP address & other setup
    setup_w5500_spi(&W5500, spi_buffer, SPIBUFSIZE, &interrupt);

    // Opens socket 0 to a TCP listening state on port 9999
    tcp_listen(&W5500.sockets[0]);
    uint8_t err;
    do {
        err = tcp_listen(&W5500.sockets[0]);
    } while (err);
    
    // Placeholder until interrupts are implemented
    for (;;) {
        _delay_ms(10000);

        // Update status
        tcp_get_connection_data(&W5500.sockets[0]);
        uart_write_P(PSTR("Socket 0 status: "));
        char status[10];
        utoa(W5500.sockets[0].status, status, 10);
        print_buffer(status, 10, 5);
        uart_write("\r\n");
    } 

    return 0;
}

void interrupt(int socketno, uint8_t interrupt) {
    uart_write_P(PSTR("Interrupt called.\r\n"));
    
    if (interrupt & (1 << CON_INT)) {
        uart_write_P(PSTR("Con_int.\r\n"));
        int length = strlen_P(index_page);
        tcp_send(&W5500.sockets[socketno], length, index_page, 1);
        tcp_disconnect(&W5500.sockets[socketno]);
    }

    if (interrupt & (1 << DISCON_INT)) {
        uart_write_P(PSTR("Discon_int.\r\n"));
        tcp_read_received(&W5500, &W5500.sockets[socketno]);
        uint8_t err;
        do {
            err = tcp_listen(&W5500.sockets[socketno]);
        } while (err);
    }

    if (interrupt & (1 << RECV_INT)) {
        uart_write_P(PSTR("Recv_int.\r\n"));
        tcp_read_received(&W5500, &W5500.sockets[socketno]);

    }

    if (interrupt & (1 << SENDOK_INT)) {
        uart_write_P(PSTR("Sendok_int.\r\n"));
    }
}
