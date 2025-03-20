#include "uart.h"
#include <util/delay.h>
#include "w5500_spi.h"

// Buffer for data read from the W5500
#define SPIBUFSIZE 100
uint8_t spi_buffer[SPIBUFSIZE] = {0,};

W5500_SPI SPI;

void interrupt(int socketno, int interrupt);

int main(void) {
    uart_init();
    stdout = &uart_output; // Redirect stdout to UART

    // IP address & other setup
    SPI = setup_w5500_spi(spi_buffer, SPIBUFSIZE, &interrupt);

    // Opens socket 0 to a TCP listening state on port 9999
    SPI.tcp_listen(SPI.sockets[0]);
    
    // Placeholder until interrupts are implemented
    for (;;) {
        _delay_ms(10000);

        // Update status
        SPI.tcp_get_connection_data(&SPI.sockets[0]);

        if (SPI.sockets[0].status == SOCK_CLOSED) {
            SPI.tcp_listen(SPI.sockets[0]);
        }
    }

    return 0;
}

void interrupt(int socketno, int interrupt) {
    printf("Interrupt received, socket %d and code %d.\n", socketno, interrupt);
    char msg[] = {"HTTP/1.1 OK\nContent-Type: text/html\n\n<!DOCTYPE html><html><body><h1>Congratulations, you've hacked into the Laatukoodit Oy mainframe.</h1></body></html>"};
    
    if (interrupt == CON_INT) {
        printf("Connected.\n");
        SPI.tcp_read_received(SPI, SPI.sockets[socketno]);
        SPI.tcp_send(SPI.sockets[socketno], sizeof(msg), msg);
        SPI.tcp_disconnect(SPI.sockets[socketno]);
    }

    if (interrupt == DISCON_INT) {
        printf("Disconnected.\n");
        SPI.tcp_read_received(SPI, SPI.sockets[socketno]);
        SPI.tcp_listen(SPI.sockets[socketno]);
    }

    if (interrupt == RECV_INT) {
        printf("Received a message.\n");
        SPI.tcp_read_received(SPI, SPI.sockets[socketno]);
        SPI.tcp_send(SPI.sockets[socketno], sizeof(msg), msg);
        SPI.tcp_disconnect(SPI.sockets[socketno]);
    }

    if (interrupt == SENDOK_INT) {
        printf("Message sent successfully.\n");
    }
}
