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
const long MR = 0x00000000, GAR0 = 0x00000100, SUBR0 = 0x00000500, SHAR0 = 0x00000900, 
    SIPR0 = 0x00000F00, PHYCFGR = 0x00002E00, VERSIONR = 0x00003900;
// Base addresses for sockets' registers (socket-no-based adjustment done later)
const long S_MR = 0x00000008, S_CR = 0x00000108, S_SR = 0x00000308, S_PORT0 = 0x00000408, 
    S_DHAR0 = 0x00000608, S_DIPR0 = 0x00000C08;
// TCP commands for W5500
const uint8_t tcpmode[] = {0x01}, sock_open[] = {0x01}, sock_listen[] = {0x02}, 
    sock_connect[] = {0x04}, sock_close[] = {0x10};


// Macros for things too small to be a function yet too weird to read
#define LOW(pin) PORTB &= ~(1 << pin)
#define HIGH(pin) PORTB |= (1 << pin)
#define WRITEOUTPUT(out) PORTB = (PORTB & ~(1 << MO)) | (out << MO)
#define READINPUT ((PINB & (1 << MI)) >> MI)


/* LOCAL*/
// Helper for socket port assignment
Socket initialise_socket(int socketno, int portno);

// Read data gets fed to the buffer uint8_t spi_buffer[]
// Clear called before new writes
void clear_spi_buffer(W5500_SPI *w5500);
void print_spi_buffer(W5500_SPI *w5500, int printlen);

// Sends header, then either uses writeByte to send the message across, or
// listens for returns on the MISO port
void transmission(W5500_SPI *w5500, long addr, int wrt, int datalen, uint8_t data[]);

// 8 cycles of shifting a byte and setting a output port based on said bit
void writeByte(uint8_t data);


/* PUBLIC*/
// Setup of various addresses
W5500_SPI setup_w5500_spi(uint8_t *buffer, int buffer_len) {  
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
    for (int i = 0; i < 8; i++) {
        sockets[i] = initialise_socket(i, 9999 + i);
    }

    W5500_SPI w5500 = {
        .spi_buf = buffer,
        .buf_len = buffer_len,
        .buf_index = 0,
        .ip_address = *ip_addr,
        .sockets = *sockets,
        
        .tcp_listen = &tcp_listen
    };

    // Pins 8, 9 & 10 assigned to output, 11 & 12 to input mode
    DDRB &= 0b11100000;
    DDRB |= (1 << CLK) | (1 << SEL) | (1 << MO);

    // Pins to 0, except for the select pin, which defaults to 1
    PORTB &= 0b11100000;
    HIGH(SEL);
 
    printf("Setting: Gateway address, subnet mask, MAC address, IP address.\n");
    // Setting of all addresses from above
    write(&w5500, GAR0, sizeof(gw_addr), gw_addr);
    write(&w5500, SUBR0, sizeof(sm_addr), sm_addr);
    write(&w5500, SHAR0, sizeof(mac_addr), mac_addr);
    write(&w5500, SIPR0, sizeof(ip_addr), ip_addr);

    return w5500;
}


void tcp_listen(W5500_SPI *w5500, Socket socket) {
    // Embed the socket number in the addresses to get the right socket
    uint8_t snomask = (socket.sockno << 5);
    long portaddr = S_PORT0 | snomask;
    long modeaddr = S_MR | snomask;
    long comaddr = S_CR | snomask;
    long statusaddr = S_SR | snomask;

    // Assign port to socket
    uint8_t port[] = {(socket.portno >> 8), socket.portno};
    write(w5500, portaddr, 2, port);

    // Set socket mode to TCP
    write(w5500, modeaddr, sizeof(tcpmode), tcpmode);
    // Open socket
    write(w5500, comaddr, sizeof(sock_open), sock_open);
    // Get the socket listening
    write(w5500, comaddr, sizeof(sock_listen), sock_listen);
    printf("Socket %d opened, status register value: ", socket.sockno);
    read(w5500, statusaddr, 1);
}


// Conducts a read operation fetching information from register(s), number of fetched bytes given by readlen
void read(W5500_SPI *w5500, long addr, int readlen) {
    uint8_t placeholder[] = {0};
    transmission(w5500, addr, 0, readlen, placeholder);
    
    // Push buffer contents out for visibility
    print_spi_buffer(w5500, readlen);
}

// Write to W5500's register(s), number of bytes written given by datalen (should be sizeof(data))
void write(W5500_SPI *w5500,long addr, int datalen, uint8_t data[]) {
    // Set write bit in header frame
    long address = addr | (1 << 2);
    transmission(w5500, address, 1, datalen, data);
}


/* LOCAL*/

Socket initialise_socket(int socketno, int portno) {
    Socket socket = {
        .sockno = socketno,
        .portno = portno,
        .mode = 0
    };
    return socket;
}


void clear_spi_buffer(W5500_SPI *w5500) { 
    printf("Clearing buffer.\n");
    for (int i = 0; i < w5500->buf_index; i++) {
        w5500->spi_buf[i] = 0;
    }
    w5500->buf_index = 0;
}


void print_spi_buffer(W5500_SPI *w5500, int printlen) {
    printf("Buffer contents: 0x%X", w5500->spi_buf[0]);
    int len = (printlen <= w5500->buf_len ? printlen : w5500->buf_len);
    for (int i = 1; i < len; i++) {
        printf("-%X", w5500->spi_buf[i]);
    }
    printf("\n");
}


// Conducts a transmission with W5500
//  Pointer to buffer and datalen (how many bytes to read) are used for write operations, 
//  data[] and datalen for write events 
void transmission(W5500_SPI *w5500, long addr, int wrt, int datalen, uint8_t data[]) {
    // Chip select low to initiate transmission
    LOW(SEL);
    
    // HEADER:
    // The first byte of the 4-byte header is irrelevant
    writeByte((uint8_t)(addr >> 16));
    writeByte((uint8_t)(addr >> 8));
    writeByte((uint8_t)addr);

    // Header sent, set output pin to low
    LOW(MO);

    // DATA:
    // Write operation
    if (wrt == 1) {       
        printf("Sending data: ");
        // Uses writeByte to push message through the pipeline byte by byte
        for (int i = 0; i < datalen; i++) {
            printf("%d", data[i]);
            printf("-");

            writeByte(data[i]);
        }
        printf("Data sent.\n");
    }
    // Read operation
    else {
        uint8_t msg = 0;

        // Removes previous data from buffer
        clear_spi_buffer(w5500);

        printf("Recording message to buffer...");

        // Check read length against available, buffer size cap if necessary
        int len = (datalen <= w5500->buf_len ? datalen : w5500->buf_len);
        // (could use some sort of error if not all can be read)
        
        for (int i = 0; i < len; i++) {
            // Reads MISO line, fills byte bit by bit
            msg = 0;
            for (int j = 0; j < 8; j++) {
                LOW(CLK);
                msg = (msg << 1) + READINPUT;
                HIGH(CLK);
            }
            // Increases buffer's index for easier read/reset
            w5500->buf_index++;
            w5500->spi_buf[i] = msg;
        }
        
        printf("Message read.\n");
    }
    
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