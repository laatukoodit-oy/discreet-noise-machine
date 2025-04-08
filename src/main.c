#include <util/delay.h>
#include <stdlib.h>
#include "w5500.h"


const char index_page[] PROGMEM = "HTTP/1.1 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><body><h1>Test</h1></body></html>";

void shuffle_interrupts();

int main(void) {
    // IP address & other setup
    setup_wizchip();

    // Initialise the server socket
    uint8_t s0_interrupt = (1 << RECV_INT) | (1 << DISCON_INT);
    initialise_socket(&Wizchip.sockets[0], TCP_MODE, 9999, s0_interrupt);

    // Opens socket 0 to a TCP listening state on port 9999
    uint8_t err;
    do {
        err = tcp_listen(&Wizchip.sockets[0]);
    } while (err);
    
    for (;;) {    
        // Check the list for a new interrupt
        if (Wizchip.interrupt_list_index == 0) {
            continue;
        }

        uart_write_P(PSTR("Interrupt called.\r\n"));

        // Extract the socket number from the list
        uint8_t interrupt = Wizchip.interrupt_list[0];
        uint8_t sockno = interrupt >> 5;

        if (sockno == DHCP_SOCKET) {
            dhcp_interrupt(&Wizchip.sockets[DHCP_SOCKET], interrupt);
            shuffle_interrupts();
            continue;
        }

        uint8_t buffer[20] = {0};

        if (interrupt & (1 << DISCON_INT)) {
            uart_write_P(PSTR("Discon_int.\r\n"));
            tcp_read_received(&Wizchip.sockets[sockno], buffer, 20);
            print_buffer(buffer, 20, 20);
            uint8_t err;
            do {
                err = tcp_listen(&Wizchip.sockets[sockno]);
            } while (err);
        }
    
        if (interrupt & (1 << RECV_INT)) {
            uart_write_P(PSTR("Recv_int.\r\n"));
            tcp_read_received(&Wizchip.sockets[sockno], buffer, 20);
            print_buffer(buffer, 20, 20);
            tcp_send(&Wizchip.sockets[sockno], sizeof(index_page), index_page, 1, 1);
            tcp_disconnect(&Wizchip.sockets[sockno]);
        }
        
        shuffle_interrupts();
    }

    return 0;
}

void shuffle_interrupts() {
    cli();
    // Shuffle everything along down the list
    for (int i = 0; i < Wizchip.interrupt_list_index; i++) {
        Wizchip.interrupt_list[i] = Wizchip.interrupt_list[i + 1];
    }
    Wizchip.interrupt_list_index--;
    Wizchip.interrupt_list[Wizchip.interrupt_list_index] = 0;
    sei();
}
