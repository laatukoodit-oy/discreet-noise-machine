/*
    A module for ethernet communication by an ATmega328P or ATtiny85 with a W5500
*/

#include "w5500.h"


// The module provides a single W5500 instance to the user
W5500 Wizchip;

/* Device initialization */
void setup_wizchip(void) {
    Wizchip.interrupt_list_index = 0;

    DHCP_Socket.sockno = 0;
    Wizchip.sockets[0] = &DHCP_Socket;
    dhcp_setup();

    TCP_Socket.sockno = 1;
    Wizchip.sockets[1] = &TCP_Socket;

    // Set up the link as a 10M half-duplex connection
    // Feed in the new config and "use these bits for configuration" setting
    set_address(PHYCFGR);
    uint8_t command = _BV(OPMD) | (PHY_HD10BTNN << OPMDC);
    write(1, &command);
    // Apply config
    command = _BV(RST);
    write(1, &command);

    // Clears the socket interrupt mask on the W5500
    set_half_address(SIMR_B);
    command = 0;
    write(1, &command);

    // Enable interrupts on the microcontroller
    setup_atthing_interrupts();

    #ifdef DEBUG
        uart_write_P(PSTR("Setup complete.\r\n"));
    #endif
}

/* Setting of various registers needed for INT0 interrupts */
void setup_atthing_interrupts(void) {
    cli();

    // Set INT0 to trogger on low
    INT_MODE = (INT_MODE & ~(_BV(ISC00) | _BV(ISC01)));
    // Enable INT0
    INT_ENABLE |= _BV(INT0);

    // Enable interrupts in general in the status register
    sei();
}

ISR(INT0_vect) {
    // Take note of the original address
    uint8_t old_pointer[] = {wizchip_address[0], wizchip_address[1], wizchip_address[2]};

    uint8_t sockets = 0, interrupts = 0;

    // Fetch interrupt register to check which socket is alerting
    set_address(SIR);
    read(&sockets, 1, 1);
    // Test for each socket
    for (uint8_t i = 0; i < SOCKETNO; i++) {
        if (EXTRACTBIT(sockets, i) == 0) {
            continue;
        }

        // Get the socket's interrupt register to see what's going on
        interrupts = 0;
        set_address(S_IR);
        embed_socket(i);
        read(&interrupts, 1, 1);

        // Write 1s to the interrupts to clear them
        write(1, &interrupts);

        // TCP sockets' tx buffer pointers are initialized on connection, so an update is necessary
        if (Wizchip.sockets[i]->mode == TCP_MODE && (interrupts & CON_INT)) {
            set_half_address(S_TX_RD_B);
            Wizchip.sockets[i]->tx_pointer = get_2_byte();
        }

        // The interrupt mask is used to set which interrupts are active,
        // so the mask can be used to filter out any extras that shouldn't cause an alert
        interrupts &= Wizchip.sockets[i]->interrupts;

        // If there's an interrupt, add it to the list
        if (interrupts == 0) {
            continue;
        }

        // Make sure you don't go out of bounds with the list size
        if (Wizchip.interrupt_list_index >= (INTERRUPT_LIST_SIZE - 1)) {
            continue;
        }

        // Embed the socket number into the three unused bits of the interrupt byte
        Wizchip.interrupt_list[Wizchip.interrupt_list_index] = (i << 5) | interrupts;
        Wizchip.interrupt_list_index++;
    }

    set_address(old_pointer[0], old_pointer[1], old_pointer[2]);
}


