
#include <stdlib.h>
#include "w5500.h"

const char content[] PROGMEM = "HTTP/1.1 OK 200\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><body><h1>Test</h1></body></html>";

void socket_init();
void check_interrupts();
void shuffle_interrupts();
uint16_t counter = 0;

int main(void) {
    // IP address & other setup
    setup_wizchip();

    socket_init();

    for (;;) {
        // Initialises server socket once an IP has been acquired
        if (DHCP.dhcp_status == FRESH_ACQUIRED) {
            socket_init();
        }
        dhcp_tracker();
        check_interrupts();
    }

    return 0;
}

void socket_init() {
    // Opens socket 1 to TCP listening state on port 9999
    tcp_socket_initialise(9999, (RECV_INT | DISCON_INT));
    uint8_t err;
    do {
        err = tcp_listen();
    } while (err);
}

void check_interrupts() {
    // Check the list for a new interrupt
    if (Wizchip.interrupt_list_index == 0) {
        return;
    }

    // Extract the socket number from the list
    uint8_t interrupt = Wizchip.interrupt_list[0];
    uint8_t sockno = interrupt >> 5;

    // DHCP operations, do not touch
    if (sockno == DHCP_SOCKET) {
        dhcp_interrupt();
        shuffle_interrupts();
        return;
    }

    /* User code below */

    uint8_t buffer[20] = {0};

    tcp_read_received(buffer, 20);
    print_buffer(buffer, 20, 20);

    if (interrupt & RECV_INT) {
        tcp_send(sizeof(content), content, OP_PROGMEM);
        tcp_disconnect();
    }

    if (interrupt & DISCON_INT) {
        uint8_t err;
        do {
            err = tcp_listen();
        } while (err);
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
