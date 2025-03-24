#include "uart.h"
#include <util/delay.h>
#include "w5500_spi.h"

// Buffer for data read from the W5500
#define SPIBUFSIZE 100
uint8_t spi_buffer[SPIBUFSIZE] = {0,};

W5500_SPI W5500;

void interrupt(int socketno, int interrupt);

int main(void) {
    uart_init();
    //stdout = &uart_output; // Redirect stdout to UART

    // IP address & other setup
    setup_w5500_spi(&W5500, spi_buffer, SPIBUFSIZE, &interrupt);

    // Opens socket 0 to a TCP listening state on port 9999
    tcp_listen(&W5500.sockets[0]);
    
    // Placeholder until interrupts are implemented
    for (;;) {
        _delay_ms(10000);

        // Update status
        tcp_get_connection_data(&W5500.sockets[0]);

        if (W5500.sockets[0].status == SOCK_CLOSED) {
            tcp_listen(&W5500.sockets[0]);
        }

        if (W5500.sockets[0].status == SOCK_CLOSE_WAIT) {
            tcp_listen(&W5500.sockets[0]);
        }
    }

    return 0;
}

void interrupt(int socketno, int interrupt) {
    char msg[] = {"HTTP/1.1 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><body><h1>Congratulations, you've hacked into the Laatukoodit Oy mainframe.</h1></body></html>"};
    
    if (interrupt == CON_INT) {
        tcp_send(&W5500.sockets[socketno], sizeof(msg), msg);
        tcp_disconnect(&W5500.sockets[socketno]);
    }

    if (interrupt == DISCON_INT) {
        tcp_read_received(&W5500, &W5500.sockets[socketno]);
        tcp_listen(&W5500.sockets[socketno]);
    }

    if (interrupt == RECV_INT) {
        tcp_read_received(&W5500, &W5500.sockets[socketno]);
    }

    if (interrupt == SENDOK_INT) {
    }
}
