/*
    Socket controls for W5500.
*/

#pragma once

#include "spi.h"
#include "w5500_addresses.h"
#include "uart.h"


// Socket interrupt bits for both mask and reading
#define CON_INT 0x01
#define DISCON_INT 0x02
#define RECV_INT 0x04
#define TIMEOUT_INT 0x08
#define SENDOK_INT 0x10

// General socket control commands
#define OPEN 0x01
#define CLOSE 0x10
#define SEND 0x20
#define RECV 0x40

// UDP socket controls
#define SEND_MAC 0x21

// Socket modes for the Sn_MR register
#define TCP_MODE 0x01
#define UDP_MODE 0x02

// Macros for things too small to be a function yet too weird to read
#define SOCKETMASK(sockno) (sockno << 5)
#define EMBEDSOCKET(base_addr, sockno) ((base_addr & 0xFFFFFF1F) | (sockno << 5))
#define EMBEDADDRESS(base_addr, new_addr) ((base_addr & 0xFF0000FF) | ((uint32_t)new_addr << 8))
#define MIN(a, b) ((a < b) ? a : b)

/* Holds data related to a single socket's operations */
typedef struct { 
    // 0 to 8
    uint8_t sockno;
    // Updated with socket_get_status
    uint8_t status;
    // TCP or UDP
    uint8_t mode;
    // The W5500 socket interrupt mask, set with initialise_socket
    uint8_t interrupts;
    // 
    uint16_t portno;
    /* The TX write pointer only gets incremented with SEND operations, so we 
    need to track it manually for compound write operations */
    uint16_t tx_pointer;
} Socket;


/* Attaches port number, interrupt mask and mode of operation to socket */
void initialise_socket(Socket *socket, uint8_t mode, uint16_t portno, uint8_t interrupt_mask);
/* Opens socket, updates struct's tx write pointer to match the current read pointer as that gets initialised with socket opens */
void socket_open(Socket *socket);
/* Self-explanatory. */
void socket_close(const Socket *socket);
/* Reads the socket's status register into the socket struct */
void socket_get_status(Socket *socket);
/*  Toggles a socket's interrupts on or off based on the socket's interrupt mask. 
    Will always include a CON_INT for TCP sockets for pointer tracking purposes, even if not requested by user. */
void toggle_socket_interrupts(const Socket *socket, bool set_on);
/* Updates the socket's TX write pointer register and commands the W5500 to transmit the contents of socket TX buffer. */
void send_send_command(Socket *socket);