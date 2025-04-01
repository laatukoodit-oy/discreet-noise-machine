#ifndef SPI_H
#define SPI_H

/*
    A module for ethernet communication by an ATmega328P or ATtiny85 with a W5500
*/

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "uart.h"
#include <stdbool.h>


#define ENABLEINT0 (INT_ENABLE |= (1 << INT0))
#define DISABLEINT0 (INT_ENABLE &= (INT_ENABLE & ~(1 << INT0)))


/* AVR */
// Device-specific pin assignments
#if defined(__AVR_ATtiny85__)
    // External interrupt    
    #define INT_MODE MCUCR
    #define INT_ENABLE GIMSK
    // SPI clock 
    #define CLK PB1
    // SPI chip select
    #define SEL PB4
    // SPI master out
    #define MO PB0
    // SPI master in
    #define MI PB3
    // W5500 interrupt signal
    #define INTR PB2
    #define INTREG DDRB
#elif defined(__AVR_ATmega328P__)
    #define INT_MODE EICRA
    #define INT_ENABLE EIMSK
    // SPI clock 
    #define CLK PB0
    // SPI chip select
    #define SEL PB1
    // SPI master out
    #define MO PB2
    // SPI master in
    #define MI PB3
    // W5500 interrupt signal
    #define INTR PD2
    #define INTREG DDRD
#else 
    #error "Not a supported microcontroller"
    // These are only here to stop the red squiggly lines
    // External interrupt    
    #define INT_MODE MCUCR
    #define INT_ENABLE GIMSK
    // SPI clock 
    #define CLK PB1
    // SPI chip select
    #define SEL PB4
    // SPI master out
    #define MO PB0
    // SPI master in
    #define MI PB3
    // W5500 interrupt signal
    #define INTR PB2
    #define INTREG DDRB
#endif


/* W5500 */
/* Addresses from the common register block, first byte is irrelevant, only the last three matter
(bytes 1 and 2 are the address, last gets used as basis for a control frame) */
// Common block - Mode register
#define MR 0x00000000ul
// Common block - Gateway address IP register
#define GAR 0x00000100ul
// Common block - Subnet mask register
#define SUBR 0x00000500ul 
// Common block - Source hardware address register
#define SHAR 0x00000900ul 
// Common block - Source IP address
#define SIPR 0x00000F00ul
// Common block - Interrupt register
#define IR 0x00001500ul
// Common block - Interrupt mask register
#define IMR 0x00001600ul
// Common block - Socket interrupt register
#define SIR 0x00001700ul
// Common block - Socket interrupt mask register
#define SIMR 0x00001800ul
// Common block - PHY configuration register
#define PHYCFGR 0x00002E00ul
// Common block - Chip version register
#define VERSIONR 0x00003900ul

// Number of bytes in the registers above 
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

/* Base addresses for sockets' registers (socket-no-based adjustment done later) */
// Socket's personal data
// Socket register block - Mode register
#define S_MR 0x00000008ul
// Socket register block - Command register
#define S_CR 0x00000108ul
// Socket register block - Interrupt register
#define S_IR 0x00000208ul
// Socket register block - Status register
#define S_SR 0x00000308ul
// Socket register block - Source port register
#define S_PORT 0x00000408ul 

// Counterparty data
// Socket register block - Destination hardware address register
#define S_DHAR 0x00000608ul
// Socket register block - Destination IP address register
#define S_DIPR 0x00000C08ul
// Socket register block - Destination port register
#define S_DPORT 0x00001008ul 
// Free space in the outgoing TX register
#define S_TX_FSR 0x00002008ul

// Socket register block - Free space in the outgoing TX buffer
#define S_TX_FSR 0x00002008ul
// Socket register block - TX buffer read pointer
#define S_TX_RD 0x00002208ul
// Socket register block - TX buffer write pointer
#define S_TX_WR 0x00002408ul
// Socket register block - RX buffer received size register
#define S_RX_RSR 0x00002608ul
// Socket register block - RX buffer read pointer
#define S_RX_RD 0x00002808ul
// Socket register block - RX buffer write pointer
#define S_RX_WR 0x00002A08ul

// Socket register block - Socket interrupt mask
#define S_IMR 0x00002C08ul

// Socket TX buffer
#define S_TX_BUF 0x00000010ul
// Socket RX buffer
#define S_RX_BUF 0x00000018ul

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

/* TCP */
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

// TCP status codes
#define SOCK_CLOSED 0x00
#define SOCK_INIT 0x13
#define SOCK_LISTEN 0x14
#define SOCK_ESTABLISHED 0x17
#define SOCK_CLOSE_WAIT 0x1C 

/* Incoming interrupts */
// Interrupt register offset values
#define CON_INT 0
#define DISCON_INT 1
#define RECV_INT 2
#define TIMEOUT_INT 3
#define SENDOK_INT 4 
// Leaves out the TCP timeout interrupt from sockets
#define INTERRUPTMASK(bit) ((bit << CON_INT) | (bit << DISCON_INT) | (bit << RECV_INT) | (bit << SENDOK_INT))


typedef struct { 
    uint8_t sockno, 
        status, 
        connected_ip_address[4], 
        connected_port[2];
    uint16_t portno;
} Socket;

#define SOCKETNO 1

typedef struct W5500_thing {
    // Pointer to any buffer used for data storage 
    uint8_t *spi_buffer,
        buf_length, 
        buf_index;
    // Who we are
    uint8_t ip_address[4];
    // Nice bits of data about what sockets are in use
    Socket sockets[SOCKETNO];   
} W5500;

extern W5500 Wizchip;

// Device initialization
void setup_w5500(W5500 *w5500, uint8_t *buffer, uint8_t buffer_len, void (*interrupt_func)(int socketno, uint8_t interrupt));

// Buffer manipulation
void clear_w5500_buffer(W5500 *w5500);
void print_buffer(uint8_t *buffer, uint8_t buffer_len, uint8_t printlen);

// Opens a port up for TCP Listen
uint8_t tcp_listen(Socket *socket);
void tcp_get_connection_data(Socket *socket);
bool tcp_send(Socket *socket, uint8_t message_len, char message[], bool progmem, bool sendnow);
void tcp_read_received(W5500 *w5500, Socket *socket);
void tcp_disconnect(Socket *socket);
void tcp_close(Socket *socket);

// Sets up the IO pins to communicate with 
void spi_init();
// Get data from W5500
void read(uint32_t addr, uint8_t *buffer, uint8_t buffer_len, uint8_t read_len);
// Write data to the W5500
void write(uint32_t addr, uint8_t data_len, uint8_t data[]);
void write_P(uint32_t addr, uint8_t data_len, uint8_t data[]);

#endif