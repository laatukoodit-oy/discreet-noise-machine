/*
    A module for ethernet communication by an ATmega328P or ATtiny85 with a W5500
*/

#include "w5500.h"


// The module provides a single W5500 instance to the user
W5500 Wizchip;

/* Device initialization */ 
void setup_wizchip(void) {  
    Wizchip.interrupt_list_index = 0;

    ASSIGN(DHCP.our_ip, 0, 0, 0, 0, 0);
    ASSIGN(DHCP.server, 0, 0, 0, 0, 0);
    ASSIGN(DHCP.submask, 0, 0, 0, 0, 0);

    for (int i = 0; i < SOCKETNO; i++) {
        Wizchip.sockets[i].sockno = i;
    }

    // Set up the link as a 100M autonegotiated connection
    // Feed in the new config and "use these bits for configuration" setting
    uint8_t command = _BV(OPMD) | (PHY_ALLAN << OPMDC);
    write(PHYCFGR, 1, &command);
    // Apply config
    command = _BV(RST);
    write(PHYCFGR, 1, &command);

    // Clears the socket interrupt mask on the W5500
    command = 0;
    write(SIMR, 1, &command);

    
    set_network();

    // Send the first DHCP discover
    assemble_macraw_frame(&Wizchip.sockets[DHCP_SOCKET]);

    // Enable interrupts on the microcontroller
    setup_atthing_interrupts();
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
    uint32_t general_interrupt_addr = SIR;
    uint32_t socket_interrupt_addr = S_IR;
    uint8_t sockets = 0, interrupts = 0;

    // Fetch interrupt register to check which socket is alerting
    read(general_interrupt_addr, &sockets, 1, SIR_LEN);
    // Test for each socket
    for (uint8_t i = 0; i < SOCKETNO; i++) {
        if (EXTRACTBIT(sockets, i) == 0) {
            continue;
        }

        // Get the socket's interrupt register to see what's going on
        interrupts = 0;
        socket_interrupt_addr = EMBEDSOCKET(socket_interrupt_addr, i);
        read(socket_interrupt_addr, &interrupts, 1, S_IR_LEN);

        // TCP sockets' tx buffer pointers are initialized on connection, so an update is necessary
        if (Wizchip.sockets[i].mode == TCP_MODE && (interrupts & CON_INT)) {
            uint32_t read_pointer_addr = S_TX_RD | SOCKETMASK(i);
            Wizchip.sockets[i].tx_pointer = get_2_byte(read_pointer_addr);
        }

        // Write 1s to the interrupts to clear them
        write(socket_interrupt_addr, 1, &interrupts);

        // The interrupt mask is used to set which interrupts are active, 
        // so the mask can be used to filter out any extras that shouldn't cause an alert
        interrupts &= Wizchip.sockets[i].interrupts;

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
}


