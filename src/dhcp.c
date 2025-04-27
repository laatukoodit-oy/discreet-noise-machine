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
Socket DHCP_Socket;


/* Message composition */
/* Initializes W5500's IP data as null, and socket 0 as MACRAW */
void setup_macraw();
/* Compiles a frame from various options, sends it to recipient. */
void send_dhcp_frame();

/* Incoming message analysis and verification */
/* Reads a message in the buffer, checks whether it is a DHCP reply of some sort and extracts relevant
information if it is. */
void parse_packet();
/* Checks whether a message's profile fits that of a DHCP reply. */
int8_t check_if_dhcp(uint16_t read_pointer, uint16_t received_amount);
/* Extracts information from a DHCP reply. */
void read_dhcp_reply(uint16_t read_pointer);

/* Reads the contents of a buffer and pushes them out the UART line */
void from_wizchip_to_uart(uint16_t message_len);

/* Writes packet lengths and IPv4 checksum into headers */
void ipv4_header_prep(uint16_t message_len);
/* Calculates a CRC-32 frame check sequence used in an ethernet frame */
void calculate_fcs(uint16_t message_len);
void shuffle_window(uint8_t *window);
void xor_window(uint8_t *window);


void dhcp_setup() {
    ASSIGN(DHCP.our_ip, 0, 0, 0, 0, 0);
    ASSIGN(DHCP.server, 0, 0, 0, 0, 0);
    ASSIGN(DHCP.submask, 0, 0, 0, 0, 0);
    DHCP.dhcp_status = DISCOVER;
    DHCP.dhcp_lease_time = 0;

    // Pushes DHCP network values (IP, server etc.) to W5500's network registers
    set_network();

    // Initialises socket 0 in MACRAW mode
    setup_macraw();

    // Send the first DHCP discover
    send_dhcp_frame();
}

/* A continuously polled function that occasionally repeats requests */
void dhcp_tracker() {
    DHCP.dhcp_lease_time++;

    switch (DHCP.dhcp_status) {
        case FRESH_ACQUIRED:
            DHCP.dhcp_status = ACQUIRED;
            __attribute__ ((fallthrough));
        case ACQUIRED:
            // Something to check remaining lease duration and re-request if needed
            if (DHCP.dhcp_lease_time == UINT32_MAX) {
                DHCP.dhcp_status = EXPIRED;
                DHCP.dhcp_lease_time = 0;
                break;
            }
            if (DHCP.dhcp_lease_time < (UINT32_MAX >> 1)) {
                break;
            }
            DHCP.dhcp_status = REQUEST;
            setup_macraw();
            send_dhcp_frame();
            break;
        // Default back to discover if something goes wrong
        default:
        // Assume that everything will have changed after expiration
        case EXPIRED:
            DHCP.dhcp_status = DISCOVER;
            setup_macraw();
            __attribute__ ((fallthrough));
        case DISCOVER:
        case REQUEST:
            // Don't spam the router
            if (DHCP.dhcp_lease_time < MESSAGE_DELAY) {
                break;
            }
            send_dhcp_frame();
            break;
    }
}

/* Interrupt handler for incoming messages */
void dhcp_interrupt() {
    if (DHCP.dhcp_status == ACQUIRED) {
        return;
    }
    parse_packet();
}

void set_network() {
    uint8_t array[6] = {MAC_ADDRESS};
    set_address(SHAR);
    write(6, array);

    set_half_address(SIPR_B);
    write(4, DHCP.our_ip);

    set_half_address(GAR_B);
    write(4, DHCP.server);

    set_half_address(SUBR_B);
    write(4, DHCP.submask);
}

void setup_macraw() {
    socket_initialise(&DHCP_Socket, MACRAW_MODE, CLIENT_PORT, RECV_INT);

    // Destination address to 255.255.255.255 for broadcast
    set_address(S_DIPR);
    embed_socket(1);
    write_singular(4, 0xFF);

    // Set the destination MAC to FF-FF-FF-FF-FF-FF for broadcast
    set_half_address(S_DHAR_B);
    write_singular(6, 0xFF);

    // Destination port to 67 for DHCP server
    uint8_t array[] = {(SERVER_PORT >> 8), SERVER_PORT};
    set_half_address(S_DPORT_B);
    write(2, array);

    socket_open(&DHCP_Socket);
    socket_toggle_interrupts(&DHCP_Socket, ON);
}


void send_dhcp_frame() {
    setup_macraw();

    uint16_t pointer = DHCP_Socket.tx_pointer;
    // Used for both tracking initial pointer position and to hold the message length
    uint16_t placeholder = DHCP_Socket.tx_pointer;

    set_address((pointer >> 8), pointer, S_TX_BUF_BLOCK);

    /* Step 1: write in the link layer + IPv4 + UDP headers */
    write_P(MACRAW_H_LEN, macraw_frame);
    pointer += MACRAW_H_LEN;

    // Send in the base frame start
    embed_pointer(pointer);
    write_P(DHCP_H_START_LEN, dhcp_frame_start);
    pointer += DHCP_H_START_LEN;

    // Fill rest of hardware address and additional options with zeroes
    embed_pointer(pointer);
    write_singular(DHCP_H_ZEROES, 0x00);
    pointer += DHCP_H_ZEROES;

    // Write in options
    embed_pointer(pointer);
    if ((DHCP.dhcp_status & 0x0F) == DISCOVER) {
        write_P(DISCOVER_OPTIONS_LEN, discover_options);
        pointer += DISCOVER_OPTIONS_LEN;
    }
    if ((DHCP.dhcp_status & 0x0F) == REQUEST) {
        write_P(REQUEST_OPTIONS_LEN, request_options);
        pointer += REQUEST_OPTIONS_LEN;

        // Write the server we're requesting the IP from to both options and SIADDR
        embed_pointer(placeholder + SIADDR_STEP);
        write(4, DHCP.server);
        embed_pointer(placeholder + MAGIC_COOKIE_STEP + COOKIE_TO_SERVER_STEP);
        write(4, DHCP.server);
    }

    // Write in our requested IP address
    embed_pointer(placeholder + REQUESTED_IP_STEP);
    write(4, DHCP.our_ip);

    // Set the write pointer back to the start of the message for checksums
    embed_pointer(placeholder);
    // Store message length in placeholder
    placeholder = pointer - placeholder;

    /* Write in IPv4 checksum and packet section lengths */
    ipv4_header_prep(placeholder);

    /* Write in the link layer footer / checksum */
    calculate_fcs(placeholder);
    pointer += 4;
    placeholder += 4;

    DHCP_Socket.tx_pointer = pointer;

    #ifdef DEBUG
        from_wizchip_to_uart(placeholder);
    #endif

    socket_send_message(&DHCP_Socket);

    DHCP.dhcp_lease_time = 0;
}


void parse_packet() {
    // Check where you left off reading
    set_address(S_RX_RD);
    uint16_t rx_pointer = get_2_byte();

    // Get the length info from the RX buffer (it's written as two bytes before the actual message)
    set_address((rx_pointer >> 8), rx_pointer, S_RX_BUF_BLOCK);
    uint8_t buf[2];
    read(buf, 2, 2);
    uint16_t received_amount = (((uint16_t)buf[0] << 8) | buf[1]);
    // Adjust pointers to account for the two extra bytes taken by the length info
    received_amount -= 2;
    rx_pointer += 2;

    #ifdef DEBUG
        print_buffer(buf, 2, 2);
        uart_write(".");
    #endif

    // Don't let a misaligned pointer trap you in a loop of always reading zero and never moving forward, or to reading massive amounts
    if (received_amount == 0 || received_amount > 0x02FF) {
        set_address(S_RX_RSR);
        received_amount = get_2_byte();
        // Update read pointer to mark the buffer as read
        socket_update_read_pointer(&DHCP_Socket, received_amount);

        return;
    }

    /* Check whether the message looks like it's DHCP and read it if it is */
    int8_t is_dhcp = check_if_dhcp(rx_pointer, received_amount);
    if (is_dhcp > 0) {
        read_dhcp_reply(rx_pointer);
    }

    #ifdef DEBUG
        print_buffer(&is_dhcp, 1, 1);
        set_address((rx_pointer >> 8), rx_pointer, S_RX_BUF_BLOCK);
        from_wizchip_to_uart(received_amount);
    #endif

    // Update read pointer to mark the segment as read
    rx_pointer += received_amount;
    socket_update_read_pointer(&DHCP_Socket, rx_pointer);
}

int8_t check_if_dhcp(uint16_t read_pointer, uint16_t received_amount) {
    set_address((read_pointer >> 8), read_pointer, S_RX_BUF_BLOCK);

    // Too short?
    if (received_amount < 294) {
        return 0;
    }

    uint8_t buffer[6] = {0};

    // If you're in the request stage, only accept an offer from one server
    if (DHCP.dhcp_status == REQUEST) {
        embed_pointer(read_pointer + UDP_SOURCE_STEP);
        read(buffer, 6, 4);
        if (memcmp(buffer, DHCP.server, 4)) {
            return -1;
        }
    }

    // Check the receiver MAC (CHADDR field in DHCP packet) to see if the message is directed to you
    read_pointer += CHADDR_STEP;
    embed_pointer(read_pointer);
    uint8_t comp[6] = {MAC_ADDRESS};
    read(buffer, 6, 6);
    print_buffer(buffer, 6, 6);
    print_buffer(comp, 6, 6);
    if (memcmp(buffer, comp, 6)) {
        return -2;
    }

    // Look for the magic cookie
    ASSIGN(comp, 0, MAGIC_COOKIE);
    read_pointer += CHADDR_TO_COOKIE_STEP;
    embed_pointer(read_pointer);
    read(buffer, 6, 4);
    if (memcmp(buffer, comp, 4)) {
        return -3;
    }

    // Message type checks
    read_pointer += COOKIE_TO_MTYPE_STEP;
    embed_pointer(read_pointer);
    read(buffer, 6, 1);
    // If your request is refused, start the discovery process over again
    if (buffer[0] == PNAK) {
        DHCP.dhcp_status = DISCOVER;
        return -4;
    }

    // Make sure the message type is something that would come after what you sent (such as discover being followed by offer)
    if (!(buffer[0] > (DHCP.dhcp_status & 0x0F))) {
        return -5;
    }

    return 1;
}

void read_dhcp_reply(uint16_t read_pointer) {
    // When the socket is in MACRAW mode, there are extra ethernet layers included to account for.
    set_address((read_pointer >> 8), read_pointer, S_RX_BUF_BLOCK);

    uint8_t data[6] = {0};

    /* Take note of the server's address and our offered IP */
    // The offered IP
    embed_pointer(read_pointer + YIADDR_STEP);
    read(data, 4, 4);
    ASSIGN(DHCP.our_ip, 0, data[0], data[1], data[2], data[3]);

    // The server doing the offering
    embed_pointer(read_pointer + SIADDR_STEP);
    read(data, 4, 4);
    ASSIGN(DHCP.server, 0, data[0], data[1], data[2], data[3]);

    if ((DHCP.dhcp_status & 0x0F) == DISCOVER) {
        DHCP.dhcp_status = REQUEST;
    }
    /* Assign network info upon a granted request */
    else if ((DHCP.dhcp_status & 0x0F) == REQUEST) {
        DHCP.dhcp_status = FRESH_ACQUIRED;
        DHCP.dhcp_lease_time = 0;

        // Get subnet mask
        embed_pointer(read_pointer + MAGIC_COOKIE_STEP + COOKIE_TO_SUBNET_STEP);
        read(DHCP.submask, 4, 4);

        socket_close(&DHCP_Socket);

        set_network();

        print_ip();
    }
}

void print_ip() {
    uint8_t array[4];

    uart_write_P(PSTR("Acquired IP address: "));
    for (uint8_t i = 0; i < 4; i++) {
        utoa(DHCP.our_ip[i], array, 10);
        print_buffer(array, sizeof(array), 3);
        array[1] = 0;
        array[2] = 0;
        uart_write(".");
    }
    uart_write("\r\n");
}

void from_wizchip_to_uart(uint16_t message_len) {
    uart_write("...");

    // Take note of the original address
    uint8_t old_address[3] = {wizchip_address[0], wizchip_address[1], wizchip_address[2]};
    uint16_t pointer = ((uint16_t)wizchip_address[0] << 8) | wizchip_address[1];

    uint8_t buffer[10] = {0};

    for (uint16_t left = message_len; left > 0;) {
        embed_pointer(pointer);
        read(buffer, 10, left);
        print_buffer(buffer, 10, left);
        pointer += MIN(10, left);
        left -= MIN(left, 10);
    }

    // Revert the original address
    set_address(old_address[0], old_address[1], old_address[2]);

    uart_write("...");
}

/* Writes length data and IPv4 checksum into the headers */
void ipv4_header_prep(uint16_t message_len) {
    // Take note of the original address
    uint8_t old_address[] = {wizchip_address[0], wizchip_address[1], wizchip_address[2]};

    uint16_t pointer = ((uint16_t)wizchip_address[0] << 8) | wizchip_address[1];

    // Adjust the pointer to account for the ethernet header
    pointer += ETH_H_LEN;

    // UDP packet length
    uint8_t data[2] = {((message_len - ETH_H_LEN - IPv4_H_LEN) >> 8), (message_len - ETH_H_LEN - IPv4_H_LEN)};
    embed_pointer(pointer + UDP_LENGTH_STEP);
    write(2, data);

    // IPv4 packet length
    data[0] = (message_len - ETH_H_LEN) >> 8;
    data[1] = message_len - ETH_H_LEN;
    embed_pointer(pointer + IPv4_LENGTH_STEP);
    write(2, data);

    /* IPv4 header checksum */
    // Data is read two bytes at a time and those two bytes are added to the sum
    uint32_t sum = 0;
    for (uint8_t i = 0; i < (IPv4_H_LEN / 2); i += 2) {
        embed_pointer(pointer + i);
        read(data, 2, 2);
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
    embed_pointer(pointer + IPv4_CHECKSUM_STEP);
    write(2, data);

    // Revert the original address
    set_address(old_address[0], old_address[1], old_address[2]);
}


/* Ethernet uses a CRC-32-calculated frame check sequence for error detection. */
/* Thank you to vafylec for the guide to CRC-32 */
void calculate_fcs(uint16_t message_len) {
    // Take note of the original address
    uint8_t old_address[] = {wizchip_address[0], wizchip_address[1], wizchip_address[2]};

    uint16_t pointer = ((uint16_t)wizchip_address[0] << 8) | wizchip_address[1];
    uint8_t window[4] = {0}, message[4] = {0};

    uint16_t step = 0;
    uint16_t read_len = 0;

    /* Go through the message, regularly XOR'ing with the polynomial */
    for (uint16_t i = 0; i < (message_len + 4);) {
        // Fetch next section of the message (4 bytes or whatever's left)
        step = MIN((message_len + 4) - i, 4);
        read_len = (((message_len - i) > message_len) ? 0 : (message_len - i));
        read_len = (read_len > 0xFF ? 0xFF : read_len);
        embed_pointer(pointer);

        ASSIGN(message, 0, 0, 0, 0, 0);
        read_reversed(message, 4, read_len);

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
    ASSIGN(window, 0, reverse_byte(~window[0]), reverse_byte(~window[1]), reverse_byte(~window[2]), reverse_byte(~window[3]));

    // Write frame check sequence at the end of the header
    embed_pointer(pointer);
    write(4, window);

    // Revert the original address
    set_address(old_address[0], old_address[1], old_address[2]);
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