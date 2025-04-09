#pragma once

#include "socket.h"
#include <stdlib.h>

/* DHCP */
// The amount of sockets reserved for DHCP use
#define DHCP_SOCKETNO 1

#define SERVER_PORT 67
#define CLIENT_PORT 68

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
// DHCP message type
#define MESSAGETYPE 0x35
#define MESSAGETYPE_LEN 0x01
#define DISCOVER 0x01
#define OFFER 0x02
#define REQUEST 0x03
#define DECLINE 0x04
#define PACK 0x05

#define END_OF_HEADER 44
#define HEADER_ZEROES 192


typedef struct {
    uint8_t dhcp_status;
    uint32_t dhcp_lease_time;
} DHCP_Client;

extern DHCP_Client DHCP;

// 
uint8_t activate_dhcp_client(Socket *socket);
void dhcp_interrupt(Socket *socket, uint8_t interrupt);