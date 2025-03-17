#ifndef SPI_H
#define SPI_H

/*
    A module for SPI communication between a W5500 and an Arduino (as per the pin assignments)
*/

#include <stdio.h>
#include <avr/io.h>

/* AVR */
// Offsets for I/O pins in the PINB register (for ATMega328)
#define CLK 0
#define SEL 1
#define MO 2
#define MI 3
#define INTR 4

/* W5500 */
// Addresses from the common register block, first byte is irrelevant, only the last three matter
// (bytes 1 and 2 are the address, last gets used as basis for a control frame)
#define MR 0x00000000 
#define GAR 0x00000100
#define SUBR 0x00000500 
#define SHAR 0x00000900 
#define SIPR 0x00000F00
#define PHYCFGR 0x00002E00
#define VERSIONR 0x00003900
// Number of bytes in registers above 
#define MR_LEN 1
#define GAR_LEN 4
#define SUBR_LEN 4
#define SHAR_LEN 6
#define SIPR_LEN 4
#define PHYCFGR_LEN 1
#define VERSIONR_LEN 1

// Base addresses for sockets' registers (socket-no-based adjustment done later)
// Socket information
#define S_MR 0x00000008
#define S_CR 0x00000108
#define S_SR 0x00000308
#define S_PORT 0x00000408 
// Counterparty data
#define S_DHAR 0x00000608
#define S_DIPR 0x00000C08
#define S_DPORT 0x00001008 
// Free space in the outgoing TX register
#define S_TX_FSR 0x00002008
// TX and RX registers' read and write pointers
#define S_TX_RD 0x00002208
#define S_TX_WR 0x00002408
#define S_RX_RSR 0x00002608
#define S_RX_RD 0x00002808
#define S_RX_WR 0x00002A08
// TX and RX registers themselves
#define S_TX_BUF 0x00000010
#define S_RX_BUF 0x00000018
// Number of bytes in the registers above 
#define S_MR_LEN 1
#define S_CR_LEN 1
#define S_SR_LEN 1
#define S_PORT_LEN 2
#define S_DHAR_LEN 6
#define S_DIPR_LEN 4
#define S_DPORT_LEN 2
#define S_TX_FSR_LEN 2
#define S_TX_RD_LEN 2
#define S_TX_WR_LEN 2
#define S_TX_RSR_LEN 2
#define S_RX_RD_LEN 2
#define S_RX_WR_LEN 2

// TCP commands for W5500
#define TCPMODE 0x01
#define OPEN 0x01
#define LISTEN 0x02
#define CONNECT 0x04
#define DISCON 0x08
#define CLOSE 0x10
#define SEND 0x20
#define RECV 0x40 

// TCP status codes
#define SOCK_CLOSED 0x00
#define SOCK_INIT 0x13
#define SOCK_LISTEN 0x14
#define SOCK_ESTABLISHED 0x17
#define SOCK_CLOSE_WAIT 0x1C 

typedef struct { 
    uint8_t sockno, status, connected_ip_address[4], connected_port[2];
    uint16_t portno;
} Socket;

typedef struct W5500_SPI_thing {
    // Pointer to any buffer used for data storage 
    uint8_t *spi_buffer;
    uint16_t buf_length, buf_index;
    // Who we are
    uint8_t ip_address[4];
    // Nice bits of data about what sockets are in use
    Socket sockets[8];   

    void (*tcp_listen)(Socket socket);
    void (*tcp_get_connection_data)(Socket *socket);
    void (*tcp_send)(Socket socket, uint16_t messagelen, char message[]);
    void (*tcp_read_received)(struct W5500_SPI_thing w5500, Socket socket);
    void (*tcp_disconnect)(Socket socket);
    void (*tcp_close)(Socket socket);
} W5500_SPI;

// Setup of various addresses
W5500_SPI setup_w5500_spi(uint8_t *buffer, uint16_t buffer_len);

// Opens a port up for TCP Listen
void tcp_listen(Socket socket);
void tcp_get_connection_data(Socket *socket);
void tcp_send(Socket socket, uint16_t messagelen, char message[]);
void tcp_read_received(W5500_SPI w5500, Socket socket);
void tcp_disconnect(Socket socket);
void tcp_close(Socket socket);

#endif