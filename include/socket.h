#pragma once

#include "spi.h"
#include "w5500_addresses.h"
#include "uart.h"


// Socket interrupt offsets for both mask and reading
#define CON_INT 0
#define DISCON_INT 1
#define RECV_INT 2
#define TIMEOUT_INT 3
#define SENDOK_INT 4

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


typedef struct { 
    uint8_t sockno, 
            status,
            // TCP or UDP
            mode, 
            // The W5500 socket interrupt mask, set with initialise_socket
            interrupts;
    uint16_t portno;
} Socket;


/* Socket */
// Attaches port number, interrupt mask and mode of operation to socket
uint8_t initialise_socket(Socket *socket, uint8_t mode, uint16_t portno, uint8_t interrupt_mask);
void socket_open(const Socket *socket);
void socket_close(const Socket *socket);
void socket_get_status(Socket *socket);
// Toggles a socket's interrupts on or off
void toggle_socket_interrupts(const Socket *socket, bool set_on);