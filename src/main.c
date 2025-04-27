#include <util/delay.h>
#include <stdlib.h>
#include "w5500.h"
#include "buzzer.h"
#include "index_html.h"

void shuffle_interrupts();
const unsigned char ok[] PROGMEM = "HTTP/1.1 200 OK\r\n\r\n";
const unsigned char not_found[] PROGMEM = "HTTP/1.1 404 Not Found\r\n\r\n";

void socket_init();
void check_interrupts();
void shuffle_interrupts();
uint16_t counter = 0;

uint8_t func_idx = 3;

void a_route() {
    set_sound_frequency(F_SOUND(1100));
    set_sound_duration(250);
    play_sound();

    if (sound_finished) {
        func_idx = 3;
    }
}


void b_route() {
    set_sound_frequency(F_SOUND(1300));
    set_sound_duration(250);
    play_sound();

    if (sound_finished) {
        func_idx = 3;
    }
}


void c_route() {
    set_sound_frequency(F_SOUND(1500));
    set_sound_duration(250);
    play_sound();

    if (sound_finished) {
        func_idx = 3;
    }
}


void d_route() {
    stop_sound();
}


void (*routes[])(void) = {
    a_route,
    b_route,
    c_route,
    d_route
};

int main(void) {
    // IP address & other setup
    setup_wizchip();

    socket_init();

    initialize_buzzer();

    for (;;) {
        routes[func_idx]();

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
        uint8_t endpoint = buffer[5] & 7;

        // Endpoint was a space -> send index_html
        if (endpoint == 0) {
            tcp_send(
                sizeof(index_html),
                index_html,
                OP_PROGMEM
            );
        }
        // Endpoint was 'a'-'e'
        else if (endpoint < 5) {
            tcp_send(
                sizeof(ok),
                ok,
                OP_PROGMEM
            );

            func_idx = endpoint - 1;
            sound_finished = false;
        }
        // Endpoint was not 'a'-'e'
        else {
            tcp_send(
                sizeof(not_found),
                not_found,
                OP_PROGMEM
            );
        }
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
