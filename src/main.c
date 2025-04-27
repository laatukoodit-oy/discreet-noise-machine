#include <util/delay.h>
#include <stdlib.h>
#include "w5500.h"
#include "buzzer.h"
#include "index_html.h"

void shuffle_interrupts();
const unsigned char ok[] PROGMEM = "HTTP/1.1 200 OK\r\n\r\n";
const unsigned char not_found[] PROGMEM = "HTTP/1.1 404 Not Found\r\n\r\n";

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

    // Initialise the server socket
    uint8_t s0_interrupt = (1 << RECV_INT) | (1 << DISCON_INT);
    initialise_socket(&Wizchip.sockets[0], TCP_MODE, 9999, s0_interrupt);

    // Opens socket 0 to a TCP listening state on port 9999
    uint8_t err;
    do {
        err = tcp_listen(&Wizchip.sockets[0]);
    } while (err);

    initialize_buzzer();

    for (;;) {
        routes[func_idx]();

        // Check the list for a new interrupt
        if (Wizchip.interrupt_list_index == 0) {
            continue;
        }

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
            tcp_read_received(&Wizchip.sockets[sockno], buffer, 20);
            uint8_t err;
            do {
                err = tcp_listen(&Wizchip.sockets[sockno]);
            } while (err);
        }

        if (interrupt & (1 << RECV_INT)) {
            tcp_read_received(
                &Wizchip.sockets[sockno],
                buffer,
                20
            );

            uint8_t endpoint = buffer[5] & 7;

            // Endpoint was a space -> send index_html
            if (endpoint == 0) {
                tcp_send(
                    &Wizchip.sockets[sockno],
                    sizeof(index_html),
                    index_html,
                    1,
                    1
                );
            }
            // Endpoint was 'a'-'e'
            else if (endpoint < 5) {
                tcp_send(
                    &Wizchip.sockets[sockno],
                    sizeof(ok),
                    ok,
                    1,
                    1
                );

                func_idx = endpoint - 1;
                sound_finished = false;
            }
            // Endpoint was not 'a'-'e'
            else {
                tcp_send(
                    &Wizchip.sockets[sockno],
                    sizeof(not_found),
                    not_found,
                    1,
                    1
                );
            }

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
