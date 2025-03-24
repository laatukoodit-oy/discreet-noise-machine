#ifndef SPI_H
#define SPI_H

/*
    A module for SPI communication between a W5500 and an Arduino (as per the pin assignments)
*/

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>



/* AVR */
// Shared values
#define CLK PB0
#define SEL PB1
// Device-specific
#if defined(__AVR_ATtiny85__)
    // External interrupt    
    #define INT_MODE MCUCR
    #define INT_ENABLE GIMSK
    #define MO PB3
    #define MI PB4
    #define INTR PB2
    #define INTREG DDRB
#elif defined(__AVR_ATmega328P__)
    #define INT_MODE EICRA
    #define INT_ENABLE EIMSK
    #define MO PB2
    #define MI PB3
    #define INTR PD2
    #define INTREG DDRD
#else 
    #error "Not a supported microcontroller"
    // These are only here to stop the red squiggly lines
    #define INT_MODE EICRA
    #define INT_ENABLE EIMSK
    #define MO PB2
    #define MI PB3
    #define INTR PD2
    #define INTREG DDRD
#endif


/* W5500 */
// Addresses from the common register block, first byte is irrelevant, only the last three matter
// (bytes 1 and 2 are the address, last gets used as basis for a control frame)
#define MR 0x00000000 
#define GAR 0x00000100
#define SUBR 0x00000500 
#define SHAR 0x00000900 
#define SIPR 0x00000F00
#define IR 0x00001500
#define IMR 0x00001600
#define SIR 0x00001700
#define SIMR 0x00001800
#define PHYCFGR 0x00002E00
#define VERSIONR 0x00003900
// Number of bytes in registers above 
#define MR_LEN 1
#define GAR_LEN 4
#define SUBR_LEN 4
#define SHAR_LEN 6
#define SIPR_LEN 4
#define IR_LEN 1
#define IMR_LEN 1
#define SIR_LEN 1
#define SIMR_LEN 1
#define PHYCFGR_LEN 1
#define VERSIONR_LEN 1

// Base addresses for sockets' registers (socket-no-based adjustment done later)
// Socket information
#define S_MR 0x00000008
#define S_CR 0x00000108
#define S_IR 0x00000208
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
// Interrupt mask
#define S_IMR 0x00002C08
// TX and RX registers themselves
#define S_TX_BUF 0x00000010
#define S_RX_BUF 0x00000018
// Number of bytes in the registers above 
#define S_MR_LEN 1
#define S_CR_LEN 1
#define S_IR_LEN 1
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
#define S_IMR_LEN 1

// TCP ethernet communication modes
#define PHY_HD10BTNN 0
#define PHY_FD10BTNN 1
#define PHY_HD100BTNN 2
#define PHY_FD100BTNN 3
#define PHY_HD100BTAN 4
#define PHY_ALLAN 7
// Phy offset
#define OPMDC 3
// Config set bit
#define RST 7
// Reset (apply config) bit
#define OPMD 6

// TCP commands for W5500
#define TCPMODE 0x01
#define OPEN 0x01
#define LISTEN 0x02
#define CONNECT 0x04
#define DISCON 0x08
#define CLOSE 0x10
#define SEND 0x20
#define RECV 0x40 

// Interrupt register offset values
#define CON_INT 0
#define DISCON_INT 1
#define RECV_INT 2
#define TIMEOUT_INT 3
#define SENDOK_INT 4 

#define INTERRUPTMASK 0b00010111

// TCP status codes
#define SOCK_CLOSED 0x00
#define SOCK_INIT 0x13
#define SOCK_LISTEN 0x14
#define SOCK_ESTABLISHED 0x17
#define SOCK_CLOSE_WAIT 0x1C 



typedef struct { 
    uint8_t sockno, 
        status, 
        connected_ip_address[4], 
        connected_port[2];
    uint16_t portno;
} Socket;

#define SOCKETNO 1

typedef struct W5500_SPI_thing {
    // Pointer to any buffer used for data storage 
    uint8_t *spi_buffer;
    uint16_t buf_length, buf_index;
    // Who we are
    uint8_t ip_address[4];
    // Nice bits of data about what sockets are in use
    Socket sockets[SOCKETNO];   
} W5500_SPI;

// Device initialization
void setup_w5500_spi(W5500_SPI *w5500, uint8_t *buffer, uint16_t buffer_len, void (*interrupt_func)(int socketno, int interrupt));

// Buffer manipulation
void clear_spi_buffer(W5500_SPI *w5500);
void print_buffer(uint8_t *buffer, uint16_t bufferlen, uint16_t printlen);

// Opens a port up for TCP Listen
uint8_t tcp_listen(Socket *socket);
void tcp_get_connection_data(Socket *socket);
void tcp_send(Socket *socket, uint16_t messagelen, char message[]);
void tcp_read_received(W5500_SPI *w5500, Socket *socket);
void tcp_disconnect(Socket *socket);
void tcp_close(Socket *socket);

#endif