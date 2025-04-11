/*
    A module for ethernet communication by an ATmega328P or ATtiny85 with a W5500
*/

#include "w5500.h"


// The module provides a single W5500 instance to the user
W5500 Wizchip;


/* Setting of various registers needed for INT0 interrupts */ 
void setup_atthing_interrupts(void);


/* Device initialization */ 
void setup_wizchip(void) {  
    uart_write_P(PSTR("Setup.\r\n"));

    Wizchip.interrupt_list_index = 0;
    Wizchip.dhcp = &DHCP;

    for (int i = 0; i < SOCKETNO; i++) {
        Wizchip.sockets[i].sockno = i;
    }

    // Clears the socket interrupt mask on the W5500
    uint8_t command = 0;
    write(SIMR, 1, &command);

    // Set up the link as a 10M half-duplex connection
    // Feed in the new config and "use these bits for configuration" setting
    command = _BV(OPMD) | (PHY_HD10BTNN << OPMDC);
    write(PHYCFGR, 1, &command);
    // Apply config
    command = _BV(RST);
    write(PHYCFGR, 1, &command);

    uint8_t ip[] = {IP_ADDRESS};
    write(SIPR, 4, ip);
    // Our IP to 0.0.0.0
    //write_singular(SIPR, 4, 0x00);

    // Mac address to 4A-36-EE-E0-34-AB
    uint8_t array[] = {MAC_ADDRESS};
    write(SHAR, 6, array);

    // Default gateway to 192.168.0.1
    /*array[0] = 0xC0;
    array[1] = 0xA8;
    array[2] = 0x00;
    array[3] = 0x01;
    write(GAR, 4, array);*/
    write_singular(GAR, 4, 0x00);

    // Default subnet mask to 255.255.255.0
    array[0] = 0xFF;
    array[1] = 0xFF;
    array[2] = 0xFF;
    array[3] = 0x00;
    write(SUBR, 4, array);

    // Get an IP address
    //activate_dhcp_client(&Wizchip.sockets[DHCP_SOCKET]);

    uart_write("Acquired IP address: ");
    read(SIPR, ip, sizeof(ip), 4);
    for (int i = 0; i < 4; i++) {
        utoa(ip[i], array, 10);
        print_buffer(array, sizeof(array), 3);
        array[1] = 0;
        array[2] = 0;
        uart_write(".");
    }
    uart_write("\r\n");

    // Enable sending of interrupt signals on the W5500 and reading them here
    setup_atthing_interrupts();

    uart_write_P(PSTR("Setup done.\r\n"));
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


