#include "dhcp.h"

const uint8_t dhcp_frame_start[END_OF_HEADER] PROGMEM = {BOOTREQUEST, HTYPE, HLEN, HOPS, XID, [10] = FLAGS, [28] = MAC_ADDRESS};
DHCP_Client DHCP;

void send_dhcp_frame(const Socket *socket);

/* DHCP */
uint8_t activate_dhcp_client(Socket *socket) {
    //uart_write_P(PSTR("Starting DHCP activation.\r\n"));

    // 
    DHCP.dhcp_status = DISCOVER;
    
    // Set up socket
    initialise_socket(socket, UDP_MODE, CLIENT_PORT, ((1 << RECV_INT) | (1 << SENDOK_INT)));

    // Prep the socket for the first broadcast transmission
    uint8_t socketmask = SOCKETMASK(socket->sockno);
    uint32_t dest_ip_address = S_DIPR | socketmask;
    uint32_t dest_mac_address = S_DHAR | socketmask;
    uint32_t dest_port_address = S_DPORT | socketmask;

    // Destination address to 255.255.255.255 for broadcast
    uint8_t dest_ip[4] = {0xC0, 0xA8, 0x00, 0xFF};
    write(dest_ip_address, 4, dest_ip);
    //write_singular(dest_ip_address, 4, 0xFF);

    // Set the destination MAC to 0xFF-FF-FF-FF-FF-FF
    write_singular(dest_mac_address, 6, 0xFF);

    // Destination port to 67 for DHCP server
    uint8_t port[] = {(SERVER_PORT >> 8), SERVER_PORT};
    write(dest_port_address, 2, port);

    socket_open(socket);
    toggle_socket_interrupts(socket, 1);

    send_dhcp_frame(socket);

    return 0;
}

void dhcp_interrupt(Socket *socket, uint8_t interrupt) {
    uart_write_P(PSTR("DHCP interrupt called: "));
    char buffer[4];
    print_buffer(itoa(interrupt, buffer, 10), 3, 3);
    uart_write("\r\n");
}

void send_dhcp_frame(const Socket *socket) {
    uint32_t tx_address = S_TX_BUF | SOCKETMASK(socket->sockno);
    uint32_t tx_wr_address = S_TX_WR | SOCKETMASK(socket->sockno);
    
    // Acquire the TX buffer write pointer
    uint16_t write_pointer = get_2_byte(tx_wr_address);

    // Embed the write pointer to the address to get a starting point
    tx_address = EMBEDADDRESS(tx_address, write_pointer);

    // Send in the base frame start
    write_P(tx_address, END_OF_HEADER, dhcp_frame_start);

    /* Make additions: */
    // Fill rest of hardware address and additional options with zeros
    uint16_t offset = write_pointer + END_OF_HEADER;
    uint32_t altered_address = EMBEDADDRESS(tx_address, offset);
    write_singular(altered_address, HEADER_ZEROES, 0x00);
    offset += HEADER_ZEROES;

    // Write in options
    // Magic cookie
    altered_address = EMBEDADDRESS(tx_address, offset);
    uint8_t option[6] = {0x63, 0x82, 0x53, 0x63, 0x00, 0x00};
    write(altered_address, 4, option);
    offset += 4;

    // The type of the message
    altered_address = EMBEDADDRESS(tx_address, offset);
    option[0] = MESSAGETYPE;
    option[1] = MESSAGETYPE_LEN;
    option[2] = DHCP.dhcp_status;
    write(altered_address, 3, option);
    offset += 3;

    // Our requested IP address 
    altered_address = EMBEDADDRESS(tx_address, offset);
    // Read the address from the W5500 and send that
    //read(SIPR, &option[2], 4, 4);
    uint8_t ip[] = {IP_ADDRESS};
    option[0] = REQUESTED_IP;
    option[1] = REQUESTED_IP_LEN;
    option[2] = ip[0];
    option[3] = ip[1];
    option[4] = ip[2];
    option[5] = ip[3];
    write(altered_address, 6, option);
    offset += 6;

    // The very important "end" option
    altered_address = EMBEDADDRESS(tx_address, offset);
    option[0] = 0xFF;
    option[1] = 0x00;
    write(altered_address, 2, option);
    offset += 2;

    // Update write pointer
    option[0] = offset >> 8;
    option[1] = offset;
    write(tx_wr_address, 2, option);

    // Check what we're sending
    offset = write_pointer;
    for (int i = 0; i < 50; i++) {
        altered_address = EMBEDADDRESS(S_TX_BUF, (offset + (uint32_t)(i * 6))) | SOCKETMASK(socket->sockno);
        read(altered_address, option, sizeof(option), 6);
        print_buffer(option, sizeof(option), 6);
    }

    // Send the frame
    uint32_t send_addr = S_CR | SOCKETMASK(socket->sockno);
    option[0] = SEND_MAC;
    write(send_addr, 1, option);
}