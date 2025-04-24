/*
    A limited DHCP client for the W5500
*/

#include "dhcp.h"

const uint8_t macraw_frame[MACRAW_H_LEN] PROGMEM = {BROADCAST_MAC, MAC_ADDRESS, IPv4, 
    IPv4_INFO, DIFFSERV, 0x00, 0x00, IPv4_ID, IPv4_FLAGS, TTL, PROTOCOL_UDP, 0x00, 0x00, NULL_IP_ADDR, BROADCAST_IP_ADDR, 
    UDP_SOURCE_PORT, UDP_DEST_PORT, 0x00, 0x00, 0x00, 0x00};
const uint8_t dhcp_frame_start[DHCP_H_START_LEN] PROGMEM = {BOOTREQUEST, HTYPE, HLEN, HOPS, XID, [10] = FLAGS, [28] = MAC_ADDRESS};
const uint8_t discover_options[DISCOVER_OPTIONS_LEN] PROGMEM = {MAGIC_COOKIE, MESSAGETYPE, DISCOVER, REQUESTED_IP, END, 0x00};
const uint8_t request_options[REQUEST_OPTIONS_LEN] PROGMEM = {MAGIC_COOKIE, MESSAGETYPE, REQUEST, REQUESTED_IP, SERVER, DOMAIN_DATA, END};

/* A single instance of DHCP Client for our use. */
DHCP_Client DHCP;


/* Message composition */
/* Initializes W5500's IP data as null, and socket 0 as MACRAW */
void setup_macraw(Socket *socket);
/* Compiles a frame from various options, sends it to recipient. */
void assemble_dhcp_frame(Socket *socket);

/* Incoming message analysis and verification */
/* Reads a message in the buffer, checks whether it is a DHCP reply of some sort and extracts relevant
information if it is. */
void parse_packet(Socket *socket);
/* Checks whether a message's profile fits that of a DHCP reply. */
bool check_if_dhcp(const Socket *socket, uint16_t read_pointer, uint16_t received_amount);
/* Extracts information from a DHCP reply. */
void read_dhcp_reply(Socket *socket, uint16_t read_pointer);

/* Reads the contents of a buffer and pushes them out the UART line */
void from_buffer_to_uart(uint32_t address, uint16_t pointer, uint16_t message_len);

/* Writes packet lengths and IPv4 checksum into headers */
void ipv4_header_prep(uint32_t address, uint16_t message_len);
/* Calculates a CRC-32 frame check sequence used in an ethernet frame */
void calculate_fcs(uint32_t address, uint16_t message_len);
void shuffle_window(uint8_t *window);
void xor_window(uint8_t *window);

/* A continuously polled function that occasionally repeats requests */
void dhcp_tracker(Socket *socket) {
    DHCP.dhcp_lease_duration++;
    
    switch (DHCP.dhcp_status) {
        case FRESH_ACQUIRED:
            DHCP.dhcp_status = ACQUIRED;
        case ACQUIRED:
            // Something to check remaining lease time and re-request if needed
            if (DHCP.dhcp_lease_duration == DHCP.dhcp_lease_time) {
                DHCP.dhcp_status = EXPIRED;
                DHCP.dhcp_lease_time = 0;
                break;
            }
            if (DHCP.dhcp_lease_duration < (DHCP.dhcp_lease_time / 2)) {
                DHCP.dhcp_lease_time++;
                break;
            }
            DHCP.dhcp_status = REREQUEST;
            assemble_dhcp_frame(socket);
            break;
        case REREQUEST:
            // Don't spam the router
            if (DHCP.dhcp_lease_duration < MESSAGE_DELAY) {
                break;
            }
            assemble_dhcp_frame(socket);
            break;
        // Assume that everything will have changed after expiration
        case EXPIRED:
        case DISCOVER:
        // Or default back to discover if something goes wrong
        default:
            DHCP.dhcp_status = DISCOVER;
        case REQUEST:
            // Don't spam the router
            if (DHCP.dhcp_lease_duration < MESSAGE_DELAY) {
                break;
            }
            assemble_macraw_frame(socket);
            break;
    }
}

/* Interrupt handler for incoming messages */
void dhcp_interrupt(Socket *socket) {
    if (DHCP.dhcp_status == ACQUIRED) {
        return;
    }
    parse_packet(socket);
}

void set_network() {
    uint8_t array[6] = {MAC_ADDRESS};
    write(SHAR, 6, array);
    write(SIPR, 4, DHCP.our_ip);
    write(GAR, 4, DHCP.server);
    write(SUBR, 4, DHCP.submask);
}

void setup_macraw(Socket *socket) {    
    // Set up socket
    socket_initialise(socket, MACRAW_MODE, CLIENT_PORT, RECV_INT);

    /* Prep the socket for the first, broadcast, transmission */
    // Mac address to 42-36-EE-E0-34-AB
    uint8_t array[] = {MAC_ADDRESS};
    write(SHAR, 6, array);
    
    // Our IP
    write_singular(SIPR, 4, 0x00);
    // Default gateway
    write_singular(GAR, 4, 0x00);
    // Default subnet mask
    write_singular(SUBR, 4, 0x00);

    // Destination address to 255.255.255.255 for broadcast
    write_singular(S_DIPR, 4, 0xFF);
    // Set the destination MAC to FF-FF-FF-FF-FF-FF
    write_singular(S_DHAR, 6, 0xFF);

    // Destination port to 67 for DHCP server
    array[0] = (SERVER_PORT >> 8);
    array[1] = SERVER_PORT;
    write(S_DPORT, 2, array);

    socket_open(socket);
    socket_toggle_interrupts(socket, 1);
}


void assemble_macraw_frame(Socket *socket) {
    setup_macraw(socket);

    uint32_t tx_address = S_TX_BUF;
    uint16_t pointer = socket->tx_pointer;

    /* Step 1: write in the link layer + IPv4 + UDP headers */
    tx_address = EMBEDADDRESS(tx_address, socket->tx_pointer);
    write_P(tx_address, MACRAW_H_LEN, macraw_frame);
    socket->tx_pointer += MACRAW_H_LEN;

    /* Write in the DHCP packet */
    assemble_dhcp_frame(socket);

    /* Write in IPv4 checksum and packet section lengths */
    ipv4_header_prep(EMBEDADDRESS(tx_address, pointer), (socket->tx_pointer - pointer));

    /* Write in the link layer footer / checksum */
    calculate_fcs(EMBEDADDRESS(tx_address, pointer), (socket->tx_pointer - pointer));
    socket->tx_pointer += 4;

    // Debug
    //from_buffer_to_uart(tx_address, pointer, (socket->tx_pointer - pointer));

    socket_send_message(socket);

    DHCP.dhcp_lease_duration = 0;
}

/* Compiles a frame from various options, sends it to recipient. */
void assemble_dhcp_frame(Socket *socket) {
    uint32_t tx_address = S_TX_BUF;
    uint16_t pointer = socket->tx_pointer;

    // Embed the write pointer to the address to get a starting point
    tx_address = EMBEDADDRESS(tx_address, socket->tx_pointer);
    
    // Send in the base frame start
    write_P(tx_address, DHCP_H_START_LEN, dhcp_frame_start);
    socket->tx_pointer += DHCP_H_START_LEN;

    // Fill rest of hardware address and additional options with zeroes
    tx_address = EMBEDADDRESS(tx_address, socket->tx_pointer);
    write_singular(tx_address, DHCP_H_ZEROES, 0x00);
    socket->tx_pointer += DHCP_H_ZEROES;

    // Set the pointer to the start of the options block
    pointer = socket->tx_pointer;

    // Write in options
    tx_address = EMBEDADDRESS(tx_address, socket->tx_pointer);
    if ((DHCP.dhcp_status & 0x0F) == DISCOVER) {
        write_P(tx_address, DISCOVER_OPTIONS_LEN, discover_options);
        socket->tx_pointer += DISCOVER_OPTIONS_LEN;
    }
    if ((DHCP.dhcp_status & 0x0F) == REQUEST) {
        write_P(tx_address, REQUEST_OPTIONS_LEN, request_options);
        socket->tx_pointer += REQUEST_OPTIONS_LEN;

        // Write the server we're requesting the IP from to both options and SIADDR
        tx_address = EMBEDADDRESS(tx_address, (pointer - DHCP_H_ZEROES - SIADDR_TO_ZEROES_STEP));
        write(tx_address, 4, DHCP.server);
        tx_address = EMBEDADDRESS(tx_address, (pointer + 15));
        write(tx_address, 4, DHCP.server);
    }

    // Write in our requested IP address
    tx_address = EMBEDADDRESS(tx_address, (pointer + 9));
    write(tx_address, 4, DHCP.our_ip);
}

void parse_packet(Socket *socket) {
    uart_write_P(PSTR("DHCP parser.\r\n"));
    
    uint32_t rx_addr = S_RX_BUF;

    // Check where you left off reading
    uint16_t rx_pointer = get_2_byte(S_RX_RD);

    // Get the length info from the RX buffer (it's written as two bytes before the actual message)
    uint8_t buf[2];
    rx_addr = EMBEDADDRESS(rx_addr, rx_pointer);
    read(rx_addr, buf, 2, 2);
    uint16_t received_amount = (((uint16_t)buf[0] << 8) + (uint16_t)buf[1]);
    // Adjust pointers to account for the two extra bytes taken by the length info
    
    // Don't let a misaligned pointer trap you in a loop of always reading zero and never moving forward
    if (received_amount < 3) {
        received_amount = get_2_byte(S_RX_RSR);
        // Update read pointer to mark the segment as read
        buf[0] = received_amount >> 8;
        buf[1] = received_amount;
        write(S_RX_RD, 2, buf);
        buf[0] = RECV;
        write(S_CR, 1, buf);
        return;
    }
    received_amount -= 2;
    rx_pointer += 2;
    
    /* Check whether the message looks like it's DHCP and read it if it is */
    if (check_if_dhcp(socket, rx_pointer, received_amount)) {
        read_dhcp_reply(socket, rx_pointer);
    }

    // Debug
    from_buffer_to_uart(rx_addr, rx_pointer, received_amount);

    if (DHCP.dhcp_status == FRESH_ACQUIRED) {
        return;
    }

    // Update read pointer to mark the segment as read
    rx_pointer += received_amount;
    buf[0] = rx_pointer >> 8;
    buf[1] = rx_pointer;
    write(S_RX_RD, 2, buf);
    buf[0] = RECV;
    write(S_CR, 1, buf);
}

bool check_if_dhcp(const Socket *socket, uint16_t read_pointer, uint16_t received_amount) {
    uint32_t address = EMBEDADDRESS(S_RX_BUF, read_pointer);

    // If the socket is in MACRAW mode, there are extra ethernet layers included to account for. 
    uint16_t offset = ((socket->mode == MACRAW_MODE) ? MACRAW_H_LEN : 0);

    // Too short?
    if (received_amount < (252 + offset)) {
        return 0;
    }

    uint8_t buffer[6] = {0};

    // If you're in the request stage, only accept an offer from one server
    if (DHCP.dhcp_status == REQUEST) {
        address = EMBEDADDRESS(address, (read_pointer + offset + SIADDR_STEP));
        read(address, buffer, 6, 4);
        if (do_arrays_differ(buffer, DHCP.server, 4)) {
            return 0;
        }
    }

    // Check the receiver MAC (CHADDR field in DHCP packet) to see if the message is directed to you
    offset += CHADDR_STEP;
    address = EMBEDADDRESS(address, (read_pointer + offset));
    uint8_t comp[6] = {MAC_ADDRESS};
    read(address, buffer, 6, 6);
    if (do_arrays_differ(buffer, comp, 6)) {
        return 0;
    }

    // Look for the magic cookie
    ASSIGN(comp, 0, MAGIC_COOKIE);   
    offset += CHADDR_TO_COOKIE_STEP;
    address = EMBEDADDRESS(address, (read_pointer + offset));
    read(address, buffer, 6, 4);
    if (do_arrays_differ(buffer, comp, 4)) {
        return 0;
    }

    // Make sure the message type follows the one you last sent (such as discover being followed by offer)
    offset += 6;
    address = EMBEDADDRESS(address, (read_pointer + offset));
    read(address, buffer, 6, 1);
    if (buffer[0] <= (DHCP.dhcp_status & 0x0F)) {
        return 0;
    }

    return 1;
}

void read_dhcp_reply(Socket *socket, uint16_t read_pointer) {
    uart_write_P(PSTR("Received a DHCP reply!\r\n"));

    // Adjust pointer if there are link layer headers involved
    read_pointer = (socket->mode == MACRAW_MODE ? (read_pointer + MACRAW_H_LEN) : read_pointer);
    
    uint8_t data[6] = {0};

    /* Take note of the server's address and our offered IP */
    // The offered IP
    read(EMBEDADDRESS(S_RX_BUF, (read_pointer + YIADDR_STEP)), data, 4, 4);
    ASSIGN(DHCP.our_ip, 0, data[0], data[1], data[2], data[3]);
    
    // The server doing the offering
    read(EMBEDADDRESS(S_RX_BUF, (read_pointer + SIADDR_STEP)), data, 4, 4);
    ASSIGN(DHCP.server, 0, data[0], data[1], data[2], data[3]);
    
    if ((DHCP.dhcp_status & 0x0F) == DISCOVER) {
        DHCP.dhcp_status = REQUEST;
    }
    /* Assign network info upon a granted request */
    else if ((DHCP.dhcp_status & 0x0F) == REQUEST) {
        // Get lease time
        read(EMBEDADDRESS(S_RX_BUF, (read_pointer + MAGIC_COOKIE_STEP + COOKIE_TO_LEASE_STEP)), data, 4, 4);
        // As we don't have a real clock and the time is in seconds, 
        // induce some semblance of delay by requiring a large number of cycles
        if (data[1] == 0) {
            ASSIGN(data, 0, 0x7F, 0xFF, 0xFF, 0xF0);
        } else {
            ASSIGN(data, 0, 0xFF, 0xFF, 0xFF, 0xF0);
        }
        DHCP.dhcp_lease_duration = ((uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 | (uint16_t)data[2] << 8 | data[3]);

        DHCP.dhcp_status = FRESH_ACQUIRED;
        DHCP.dhcp_lease_duration = 0;

        // Get subnet mask
        read(EMBEDADDRESS(S_RX_BUF, (read_pointer + MAGIC_COOKIE_STEP + COOKIE_TO_SUBNET_STEP)), DHCP.submask, 4, 4);

        // Reset Wizchip
        socket_close(socket);
        write_singular(MR, 1, 0x80);

        set_network();

        print_ip();

        // Switch the socket to UDP
        socket_initialise(socket, UDP_MODE, CLIENT_PORT, RECV_INT);
        socket_open(socket);
    }
}

void print_ip() {
    uint8_t array[4];

    uart_write("Acquired IP address: ");
    for (uint8_t i = 0; i < 4; i++) {
        utoa(DHCP.our_ip[i], array, 10);
        print_buffer(array, sizeof(array), 3);
        array[1] = 0;
        array[2] = 0;
        uart_write(".");
    }
    uart_write("\r\n");
}

void from_buffer_to_uart(uint32_t address, uint16_t pointer, uint16_t message_len) {
    uart_write("...");

    uint8_t buffer[10] = {0};
        
    for (uint16_t left = message_len; left > 0;) {
        address = EMBEDADDRESS(address, pointer);
        read(address, buffer, 10, left);
        print_buffer(buffer, 10, left);
        pointer += MIN(10, left);
        left -= MIN(left, 10);
    }

    uart_write("---");
}

/* Writes length data and IPv4 checksum into the headers */
void ipv4_header_prep(uint32_t address, uint16_t message_len) {
    uint16_t pointer = (address & 0x00FFFF00ul) >> 8;
    // Adjust the pointer to account for the ethernet header
    pointer += ETH_H_LEN;

    // UDP packet length
    uint8_t data[2] = {((message_len - ETH_H_LEN - IPv4_H_LEN) >> 8), (message_len - ETH_H_LEN - IPv4_H_LEN)};
    address = EMBEDADDRESS(address, (pointer + UDP_LENGTH_STEP));
    write(address, 2, data);
    
    // IPv4 packet length
    data[0] = (message_len - ETH_H_LEN) >> 8;
    data[1] = message_len - ETH_H_LEN;
    address = EMBEDADDRESS(address, (pointer + IPv4_LENGTH_STEP));
    write(address, 2, data);

    /* IPv4 header checksum */
    // Data is read two bytes at a time and those two bytes are added to the sum
    uint32_t sum = 0;
    for (uint8_t i = 0; i < (IPv4_H_LEN / 2); i += 2) {
        address = EMBEDADDRESS(address, (pointer + i));
        read(address, data, 2, 2);
        sum += ((uint16_t)data[0] << 8) + data[1];
    }

    // The upper half of the sum is added to the lower half until the upper half is empty
    while ((sum & 0xFFFF0000) > 0) {
        sum = ((sum & 0xFFFF0000) >> 16) + (sum & 0x0000FFFF);
    }

    // The result is complemented
    sum ^= 0xFFFFFFFF;

    // And finally the checksum is written in
    data[0] = sum >> 8;
    data[1] = sum;
    address = EMBEDADDRESS(address, (pointer + IPv4_CHECKSUM_STEP));
    write(address, 2, data);
}


/* Ethernet uses a CRC-32-calculated frame check sequence for error detection. */
/* Thank you to vafylec for the guide to CRC-32 */
void calculate_fcs(uint32_t address, uint16_t message_len) {
    uint16_t pointer = (address & 0x00FFFF00ul) >> 8;
    uint8_t window[4] = {0}, message[4] = {0};
    
    uint16_t step = 0;
    uint16_t read_len = 0;

    /* Go through the message, regularly XOR'ing with the polynomial */
    for (uint16_t i = 0; i < (message_len + 4);) {
        // Fetch next section of the message (4 bytes or whatever's left)
        step = MIN((message_len + 4) - i, 4);
        read_len = (((message_len - i) > message_len) ? 0 : (message_len - i));
        read_len = (read_len > 0xFF ? 0xFF : read_len);
        address = EMBEDADDRESS(address, pointer);

        ASSIGN(message, 0, 0, 0, 0, 0);
        read_reversed(address, message, 4, read_len);

        // The very first bytes are inverted
        if (i == 0) {
            ASSIGN(window, 0, ~message[0], ~message[1], ~message[2], ~message[3]);
            pointer += step;
            i += step;
            continue;
        }

        pointer += MIN(read_len, step);
        i += step;

        for (uint8_t j = 0; j < step; j++) {
            for (uint8_t k = 8; k > 0; k--) {
                // XOR only when the first bit of the window is a zero
                if (window[0] & 0x80) {
                    shuffle_window(window);
                    window[3] |= EXTRACTBIT(message[j], (k - 1));
                    xor_window(window);
                }
                // Else just move the window along
                else {
                    shuffle_window(window);
                    window[3] |= EXTRACTBIT(message[j], (k - 1));
                }
            }
        } 
    }

    // XOR and reverse the bytes
    window[0] = reverse_byte(~window[0]);
    window[1] = reverse_byte(~window[1]);
    window[2] = reverse_byte(~window[2]);
    window[3] = reverse_byte(~window[3]);
    
    // Write frame check sequence at the end of the header
    address = EMBEDADDRESS(address, pointer);
    write(address, 4, window);
}

void shuffle_window(uint8_t *window) {
    for (uint8_t i = 0; i < 3; i++) {
        window[i] <<= 1;
        window[i] |= EXTRACTBIT(window[i + 1], 7);
    }
    window[3] <<= 1;
}

void xor_window(uint8_t *window) {
    window[0] ^= 0x04;
    window[1] ^= 0xC1;
    window[2] ^= 0x1D;
    window[3] ^= 0xB7;
}