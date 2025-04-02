#ifndef W5500_H
#define W5500_H

/*
    A module for ethernet communication by an ATmega328P or ATtiny85 with a W5500
*/

#include <avr/interrupt.h>
#include <stdbool.h>

#include "uart.h"
#include "w5500_spi.h"
#include "w5500_addresses.h"


/* User-relevant macros below */

// Number of sockets available for use in the Wizchip (max. 7 as one is taken by the DHCP client) 
#define SOCKETNO 1

/* User-relevant macros above */


// Ethernet communication modes
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

/* TCP */
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

// TCP commands for W5500
#define TCPMODE 0x01
#define OPEN 0x01
#define LISTEN 0x02
#define CONNECT 0x04
#define DISCON 0x08
#define CLOSE 0x10
#define SEND 0x20
#define RECV 0x40


typedef struct { 
    uint8_t sockno, 
        status, 
        connected_ip_address[4], 
        connected_port[2];
    uint16_t portno;
} Socket;

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

// 
extern W5500 Wizchip;

// Device initialization
void setup_wizchip(uint8_t *buffer, uint8_t buffer_len, void (*interrupt_func)(int socketno, uint8_t interrupt));

// Buffer manipulation
void clear_wizchip_buffer(void);
void print_buffer(uint8_t *buffer, uint8_t buffer_len, uint8_t printlen);

// Opens a port up for TCP Listen
uint8_t tcp_listen(Socket *socket);
void tcp_get_connection_data(Socket *socket);
bool tcp_send(Socket *socket, uint8_t message_len, char message[], bool progmem, bool sendnow);
void tcp_read_received(Socket *socket);
void tcp_disconnect(Socket *socket);
void tcp_close(Socket *socket);

#endif