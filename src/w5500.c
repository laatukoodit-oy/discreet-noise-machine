/*
    A module for SPI communication between a W5500 and either an ATmega328P or ATtiny85
*/

#include "w5500.h"


W5500 Wizchip;


// Setting of various registers needed for INT0 interrupts
void setup_atthing_interrupts(void);


// Device initialization
uint8_t setup_wizchip(void) {
    //uart_write_P(PSTR("Setup.\r\n"));

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
    command = (1 << OPMD) | (PHY_HD10BTNN << OPMDC);
    write(PHYCFGR, 1, &command);
    // Apply config
    command = (1 << RST);
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
    read(SIPR, array, sizeof(array), 4);
    print_buffer(array, sizeof(array), 4);
    uart_write("\r\n");

    // Enable sending of interrupt signals on the W5500 and reading them here
    setup_atthing_interrupts();

    return 0;
}

void setup_atthing_interrupts(void) {
    cli();

    // Set INT0 to trogger on low
    INT_MODE = (INT_MODE & ~((1 << ISC00) | (1 << ISC01)));
    // Enable INT0
    INT_ENABLE |= (1 << INT0);
    // Enable interrupts in general in the status register
    sei();
}

ISR(INT0_vect) {
    //uart_write_P(PSTR("INT0.\r\n"));

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

        print_buffer(&interrupts, 1, 1);

        // The interrupt mask is used to set which interrupts are active,
        // so the mask can be used to filter out any extras that shouldn't cause an alert
        interrupts &= Wizchip.sockets[i].interrupts;

        if (interrupts == 0) {
            continue;
        }

        // Write 1s to the interrupts to clear them
        write(socket_interrupt_addr, 1, &interrupts);

        // Make sure you don't go out of bounds with the list size
        if (Wizchip.interrupt_list_index >= (INTERRUPT_LIST_SIZE - 1)) {
            continue;
        }

        // Embeds the socket number into the three unused bits of the interrupt byte
        Wizchip.interrupt_list[Wizchip.interrupt_list_index] = (i << 5) | interrupts;
        // Pass interrupt info onto the list
        Wizchip.interrupt_list_index++;
    }
}


