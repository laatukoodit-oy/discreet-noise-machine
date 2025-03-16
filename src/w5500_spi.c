/*
    A module for SPI communication between a W5500 and an Arduino (as per the pin assignments, the rest might be more universal)
*/

#include "w5500_spi.h"


/* ARDUINO */
// Offsets for I/O pins in the PINB register
const int CLK = 0, SEL = 1, MO= 2, MI = 3, INTR = 4;


/* W5500 */
// Addresses from the common register block, first byte is irrelevant, only the last three matter
// (bytes 1 and 2 are the address, last gets used as basis for a control frame)
const uint32_t MR = 0x00000000, GAR = 0x00000100, SUBR = 0x00000500, SHAR = 0x00000900, 
    SIPR = 0x00000F00, PHYCFGR = 0x00002E00, VERSIONR = 0x00003900;
// Number of bytes in register 
const uint8_t MR_LEN = 1, GAR_LEN = 4, SUBR_LEN = 4, SHAR_LEN = 6, 
    SIPR_LEN = 4, PHYCFGR_LEN = 1, VERSIONR_LEN = 1;

// Base addresses for sockets' registers (socket-no-based adjustment done later)
// Socket information
const uint32_t S_MR = 0x00000008, S_CR = 0x00000108, S_SR = 0x00000308, S_PORT = 0x00000408, 
    // Counterparty data
    S_DHAR = 0x00000608, S_DIPR = 0x00000C08, S_DPORT = 0x00001008, 
    // Free space in the outgoing TX register
    S_TX_FSR = 0x00002008,
    // TX and RX registers' read and write pointers
    S_TX_RD = 0x00002208, S_TX_WR = 0x00002408, S_TX_RSR = 0x00002608, S_RX_RD = 0x00002808, S_RX_WR = 0x00002A08,
    // TX and RX registers themselves
    S_TX_BUF = 0x00000010, S_RX_BUF = 0x00000018; 
const uint8_t S_MR_LEN = 1, S_CR_LEN = 1, S_SR_LEN = 1, S_PORT_LEN = 2, 
    S_DHAR_LEN = 6, S_DIPR_LEN = 4, S_DPORT_LEN = 2, 
    S_TX_FSR_LEN = 2,
    S_TX_RD_LEN = 2, S_TX_WR_LEN = 2, S_TX_RSR_LEN = 2, S_RX_RD_LEN = 2, S_RX_WR_LEN = 2;

// TCP commands for W5500
const uint8_t TCPMODE[] = {0x01}, OPEN[] = {0x01}, LISTEN[] = {0x02}, 
    CONNECT[] = {0x04}, DISCON[] = {0x08}, CLOSE[] = {0x10}, SEND[] = {0x20};


// Macros for things too small to be a function yet too weird to read
#define LOW(pin) PORTB &= ~(1 << pin)
#define HIGH(pin) PORTB |= (1 << pin)
#define WRITEOUTPUT(out) PORTB = (PORTB & ~(1 << MO)) | (out << MO)
#define READINPUT ((PINB & (1 << MI)) >> MI)


/* LOCAL*/
// Helper for socket port assignment
void initialise_socket(Socket *socket, uint8_t socketno, uint16_t portno);

void clear_spi_buffer(W5500_SPI *w5500);
void print_buffer(uint8_t *buffer, uint16_t bufferlen, uint16_t printlen);

// Get data from W5500
void read(uint32_t addr, uint8_t *buffer, uint16_t bufferlen, uint16_t readlen);
// Write data to the W5500
void write(uint32_t addr, uint16_t datalen, uint8_t data[]);

// Sends header to start of transmission
void start_transmission(uint32_t addr);
// Chip select low
void end_transmission();

// 8 cycles of shifting a byte and setting a output port based on said bit
void writeByte(uint8_t data);


/* PUBLIC*/
// Setup of various addresses
W5500_SPI setup_w5500_spi(uint8_t *buffer, uint16_t buffer_len) {  
    printf("W5500 setup started.\n");

    // Gateway address to 192.168.0.1
    uint8_t gw_addr[] = {0xC0, 0xA8, 0x00, 0x01};
    // Subnet mask to 255.255.255.0
    uint8_t sm_addr[] = {0xFF, 0xFF, 0xFF, 0x00};
    // Mac address to 4A-36-EE-E0-34-AB
    uint8_t mac_addr[] = {0x4A, 0x36, 0xEE, 0xE0, 0x34, 0xAB};
    // IP address to 192.168.0.15 (for group 15)
    uint8_t ip_addr[] = {0xC0, 0xA8, 0x00, 0x0F};

    Socket sockets[8];
    for (uint16_t i = 0; i < 8; i++) {
        Socket newsock;
        initialise_socket(&newsock, i, 9999 + i);
        sockets[i] = newsock;
    }

    W5500_SPI w5500 = {
        .spi_buf = buffer,
        .buf_len = buffer_len,
        .buf_index = 0,
        .ip_address = {*ip_addr},
        .sockets = {*sockets},
        
        .tcp_listen = &tcp_listen,
        .tcp_get_connection_data = &tcp_get_connection_data,
        .tcp_send = &tcp_send,
        .tcp_close = &tcp_close,
        .tcp_disconnect = &tcp_disconnect
    };

    // Pins 8, 9 & 10 assigned to output, 11 & 12 to input mode
    DDRB &= 0b11100000;
    DDRB |= (1 << CLK) | (1 << SEL) | (1 << MO);

    // Pins to 0, except for the select pin, which defaults to 1
    PORTB &= 0b11100000;
    HIGH(SEL);
 
    // Setting of all addresses from above
    write(GAR, sizeof(gw_addr), gw_addr);
    write(SUBR, sizeof(sm_addr), sm_addr);
    write(SHAR, sizeof(mac_addr), mac_addr);
    write(SIPR, sizeof(ip_addr), ip_addr);

    return w5500;
}


/* TCP Connections */
// 
void tcp_listen(Socket socket) {
    // Embed the socket number in the addresses to hit right register block
    uint8_t snomask = (socket.sockno << 5);
    uint32_t portaddr = S_PORT | snomask;
    uint32_t modeaddr = S_MR | snomask;
    uint32_t comaddr = S_CR | snomask;

    printf("Opening port %d for socket %d, mode TCP listen.\n", socket.portno, socket.sockno);

    // Assign port to socket
    uint8_t port[] = {(socket.portno >> 8), socket.portno};
    write(portaddr, S_PORT_LEN, port);

    // Set socket mode to TCP
    write(modeaddr, S_MR_LEN, TCPMODE);
    // Open socket
    write(comaddr, S_CR_LEN, OPEN);
    // Get the socket listening
    write(comaddr, S_CR_LEN, LISTEN);
    
    // Check status
    tcp_get_connection_data(&socket);
}

void tcp_get_connection_data(Socket *socket) {
    // Embed the socket number in the address to hit right register block
    uint8_t snomask = (socket->sockno << 5);
    uint32_t statusaddr = S_SR | snomask;

    read(statusaddr, &socket->status, S_SR_LEN, S_SR_LEN);

    printf("Connection data for socket %d: status %d", socket->sockno, socket->status);

    if (socket->status == SOCK_ESTABLISHED) {
        uint32_t ipaddr = S_DIPR | snomask;
        uint32_t portaddr = S_DPORT | snomask;
        read(ipaddr, socket->connected_ip_address, sizeof(socket->connected_ip_address), S_DIPR_LEN);
        read(portaddr, socket->connected_port, sizeof(socket->connected_port), S_DPORT_LEN);
        printf(", connected to IP %u.%u.%u.%u and port %d", socket->connected_ip_address[0], socket->connected_ip_address[1], socket->connected_ip_address[2], socket->connected_ip_address[3], (socket->connected_port[0] + socket->connected_port[1]));
    }

    printf(".\n");
}

void tcp_send(Socket socket, uint16_t messagelen, char message[]) {
    
    // Embed the socket number in the addresses to hit right register block
    uint8_t snomask = (socket.sockno << 5);
    uint32_t txfsraddr = S_TX_FSR | snomask;
    uint32_t txwraddr = S_TX_WR | snomask;
    uint32_t txaddr = S_TX_BUF | snomask;
    uint32_t sendaddr = S_CR | snomask;

    // If the whole message can't fit in the register, send it in multiple pieces
    uint16_t message_left = messagelen;    
    do {
        // Get the TX buffer write pointer, adjust address to start writing from that point
        uint8_t txwr[] = {0, 0}; 
        read(txwraddr, txwr, S_TX_WR_LEN, S_TX_WR_LEN);
        txaddr = ((txaddr & 0xFF0000FF) | ((uint32_t)txwr[0] << 16) | ((uint16_t)txwr[1] << 8));

        // Check the free space in the TX buffer (which may be unreliable, so check until it stabilises)
        uint8_t txfsr1[] = {0, 0}, txfsr2[] = {0, 0}; 
        do {
            txfsr2[0] = txfsr1[0];
            txfsr2[1] = txfsr1[1];
            read(txfsraddr, txfsr1, S_TX_FSR_LEN, S_TX_FSR_LEN);
        } while (txfsr1[0] != txfsr2[0] || txfsr1[1] != txfsr2[1]);
        uint16_t send_amount = ((uint16_t)txfsr2[0] << 8 | txfsr2[1]);
        
        // Compare free space to the remaining message, send as much as you can
        send_amount = (send_amount < message_left) ? send_amount : message_left ;
        write(txaddr, send_amount, &message[messagelen - message_left]);
        message_left -= send_amount;

        // Write a new value to the TX buffer write pointer to match post-input situation
        uint16_t ntxwr = (send_amount + ((uint16_t)txwr[0] << 8) + txwr[1]);
        uint8_t new_txwr[] = {ntxwr >> 8, ntxwr};
        write(txwraddr, S_TX_WR_LEN, new_txwr);
        
        // Send the "send" command to pass the message to the other party
        write(sendaddr, S_CR_LEN, SEND);
        
    } while (message_left > 0);
}

void tcp_disconnect(Socket socket) {
    // Embed the socket number in the address to hit right register block
    uint8_t snomask = (socket.sockno << 5);
    uint32_t comaddr = S_CR | snomask;

    printf("Closing TCP connection in socket %d.\n", socket.sockno);

    write(comaddr, S_CR_LEN, DISCON);    
}

void tcp_close(Socket socket) {
    // Embed the socket number in the address to hit right register block
    uint8_t snomask = (socket.sockno << 5);
    uint32_t comaddr = S_CR | snomask;

    printf("Closing socket %d.\n", socket.sockno);

    write(comaddr, S_CR_LEN, CLOSE);
}


/* LOCAL*/
/* Socket operations */
void initialise_socket(Socket *socket, uint8_t socketno, uint16_t portno) {
    socket->sockno = socketno;
    socket->portno = portno;
    socket->status = 0;
}


/* Buffer manipulation */
void clear_spi_buffer(W5500_SPI *w5500) { 
    printf("Clearing buffer.\n");
    for (uint16_t i = 0; i < w5500->buf_index; i++) {
        w5500->spi_buf[i] = 0;
    }
    w5500->buf_index = 0;
}

void print_buffer(uint8_t *buffer, uint16_t bufferlen, uint16_t printlen) {
    printf("Buffer contents: 0x%X", buffer[0]);
    uint16_t len = (printlen <= bufferlen ? printlen : bufferlen);
    for (uint16_t i = 1; i < len; i++) {
        printf("-%X", buffer[i]);
    }
    printf("\n");
}


/* Transmissions */
// Conducts a read operation fetching information from register(s), number of fetched bytes given by readlen
void read(uint32_t addr, uint8_t *buffer, uint16_t bufferlen, uint16_t readlen) {
    // Send header
    start_transmission(addr);

    uint8_t byte = 0;

    // Check read length against available buffer size, cap if necessary
    uint16_t len = (readlen <= bufferlen ? readlen : bufferlen);
    // (could use some sort of error if not all can be read)
    
    for (uint16_t i = 0; i < len; i++) {
        // Reads MISO line, fills byte bit by bit
        byte = 0;
        for (int j = 0; j < 8; j++) {
            LOW(CLK);
            byte = (byte << 1) + READINPUT;
            HIGH(CLK);
        }
        buffer[(uint8_t)i] = byte;
    }
    end_transmission();

    // Push buffer contents out for visibility
    print_buffer(buffer, bufferlen, readlen);
}

// Write to W5500's register(s), number of bytes written given by datalen (should be sizeof(data))
void write(uint32_t addr, uint16_t datalen, uint8_t data[]) {
    // Set write bit in header frame
    uint32_t address = addr | (1 << 2);
    // Send header
    start_transmission(address);
    
    printf("Sending data: ");
    // Uses writeByte to push message through the pipeline byte by byte
    for (uint16_t i = 0; i < datalen; i++) {
        printf("%X-", data[i]);

        writeByte(data[i]);
    }
    printf("Data sent.\n");

    end_transmission();
}

void start_transmission(uint32_t addr) {
    // Chip select low to initiate transmission
    LOW(SEL);
    
    // HEADER:
    // The first byte of the 4-byte header is irrelevant
    writeByte((uint8_t)(addr >> 16));
    writeByte((uint8_t)(addr >> 8));
    writeByte((uint8_t)addr);

    // Header sent, set output pin to low
    LOW(MO);
}

void end_transmission() {
    // Chip select high to end transmission
    HIGH(SEL);
}

// Shift bits from MSB to LSB to the MOSI pipe, cycle clock to send
void writeByte(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        LOW(CLK);
        WRITEOUTPUT((data & (1 << i)) >> i);
        HIGH(CLK);
    }
}