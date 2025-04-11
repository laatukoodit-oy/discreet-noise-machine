/*
    A limited DHCP client for the W5500
*/

#pragma once

#include "socket.h"
#include <stdlib.h>


// The number of sockets reserved for DHCP use
#define DHCP_SOCKETNO 1

// Port numbers in DHCP message use
#define SERVER_PORT 67
#define CLIENT_PORT 68

/* Parts of the message header */
#define BOOTREQUEST 0x01
#define BOOTREPLY 0x02
#define HTYPE 0x01
#define HLEN 0x06
#define HOPS 0x00
#define FLAGS 0x80, 0x00
// An identifier of a given message chain, preferably randomised
#define XID 0x23, 0x87, 0x13, 0x65
// The preferred/default IP address for us
#define IP_ADDRESS 0xC0, 0xA8, 0x00, 0x0F
// Our MAC address
#define MAC_ADDRESS 0x4A, 0x36, 0xEE, 0xE0, 0x34, 0xAB
/* Options */
// IP
#define REQUESTED_IP 0x32
#define REQUESTED_IP_LEN 0x04
// DHCP message type identifier
#define MESSAGETYPE 0x35
#define MESSAGETYPE_LEN 0x01
// Message types themselves
#define DISCOVER 0x01
#define OFFER 0x02
#define REQUEST 0x03
#define DECLINE 0x04
#define PACK 0x05

// Lengths of different sections
#define END_OF_HEADER 44
#define HEADER_ZEROES 192

/*  Holds data related to lease negotiations. 
    Most things are stored in the W5500 itself to save memory (such as our and the server's IP addresses) */
typedef struct {
    // The phase of a DHCP negotiation we're in
    uint8_t dhcp_status;
    // Timestamp for when we got our IP address
    uint32_t dhcp_lease_time;
    // How long we can keep the requested IP address
    uint32_t dhcp_lease_duration;
} DHCP_Client;

/* A single instance of DHCP Client for our use. */
extern DHCP_Client DHCP;

/* Sets up the socket used for transmissions and starts the IP address lease process. */
uint8_t activate_dhcp_client(Socket *socket);
/* Interrupt handler for incoming messages */
void dhcp_interrupt(Socket *socket, uint8_t interrupt);