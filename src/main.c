#include <util/delay.h>
#include <stdlib.h>
#include "w5500.h"

const char headers[] PROGMEM = "HTTP/1.1 OK\r\nContent-Type: text/html\r\n\r\n";
const char content[] PROGMEM = "<!DOCTYPE html><html><body><h1>Test</h1></body></html>";

//const char content[] PROGMEM = "HTTP/1.1 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><body><h1>Test</h1></body></html>";

void check_interrupts();
void shuffle_interrupts();

int main(void) {
    // IP address & other setup
    setup_wizchip();

    // Initialise the server socket
    uint8_t s0_interrupt = RECV_INT | DISCON_INT;
    tcp_initialise_socket(&Wizchip.sockets[0], 9999, s0_interrupt);

    // Opens socket 0 to a TCP listening state on port 9999
    uint8_t err;
    do {
        err = tcp_listen(&Wizchip.sockets[0]);
    } while (err);
    
    for (;;) {    
        check_interrupts();
    }

    return 0;
}

void check_interrupts() {
    // Check the list for a new interrupt
    if (Wizchip.interrupt_list_index == 0) {
        return;
    }

    uart_write_P(PSTR("Interrupt called.\r\n"));

    // Extract the socket number from the list
    uint8_t interrupt = Wizchip.interrupt_list[0];
    uint8_t sockno = interrupt >> 5;

    // DHCP operations, do not touch
    if (sockno == DHCP_SOCKET) {
        dhcp_interrupt(&Wizchip.sockets[DHCP_SOCKET], interrupt);
        shuffle_interrupts();
        return;
    }


    /* User code below */

    uint8_t buffer[20] = {0};

    if (interrupt & RECV_INT) {
        uart_write_P(PSTR("Recv_int.\r\n"));
        tcp_read_received(&Wizchip.sockets[sockno], buffer, 20);
        print_buffer(buffer, 20, 20);
        tcp_send(&Wizchip.sockets[sockno], sizeof(headers), headers, (OP_PROGMEM | OP_HOLDBACK));
        tcp_send(&Wizchip.sockets[sockno], sizeof(content), content, OP_PROGMEM);
        tcp_disconnect(&Wizchip.sockets[sockno]);
        uart_write_P(PSTR("Recv_int over.\r\n"));
    }

    if (interrupt & DISCON_INT) {
        uart_write_P(PSTR("Discon_int.\r\n"));
        tcp_read_received(&Wizchip.sockets[sockno], buffer, 20);
        print_buffer(buffer, 20, 20);
        uint8_t err;
        do {
            err = tcp_listen(&Wizchip.sockets[sockno]);
        } while (err);
        uart_write_P(PSTR("Discon_int over.\r\n"));
    }

    /* User code above */

    shuffle_interrupts();
}

void shuffle_interrupts() {
    cli();
    // Shuffle everything along down the list
    for (int i = 0; i < Wizchip.interrupt_list_index; i++) {
        Wizchip.interrupt_list[i] = Wizchip.interrupt_list[i + 1];
    }
    Wizchip.interrupt_list[Wizchip.interrupt_list_index] = 0;
    Wizchip.interrupt_list_index--;
    sei();
}
