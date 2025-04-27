/*
    A limited DHCP client for the W5500
*/

#pragma once

#include "socket.h"
#include <stdlib.h>
#include "string.h"

// Lengths of different sections
#define ETH_H_LEN 14u
#define ETH_FOOTER_LEN 4u
#define IPv4_H_LEN 20u
#define UDP_H_LEN 8u
#define MACRAW_H_LEN (ETH_H_LEN + IPv4_H_LEN + UDP_H_LEN)
#define DHCP_H_START_LEN 34u
#define DHCP_H_ZEROES 202u
#define DISCOVER_OPTIONS_LEN 16u
#define REQUEST_OPTIONS_LEN 26u
#define DHCP_MESSAGE_LEN (DHCP_H_START_LEN + DHCP_H_ZEROES + DISCOVER_OPTIONS_LEN)
#define UDP_TOTAL_LEN (DHCP_MESSAGE_LEN + UDP_H_LEN)
#define IPv4_TOTAL_LEN (UDP_TOTAL_LEN + IPv4_H_LEN)
#define DHCP_TOTAL_LEN (ETH_H_LEN + IPv4_TOTAL_LEN + ETH_FOOTER_LEN)

/* Basic IP components */
// The preferred/default IP address for us
#define IP_ADDRESS 0xC0, 0xA8, 0x00, 0x0F
// Our MAC address
#define MAC_ADDRESS 0x42, 0x36, 0xEE, 0xE0, 0x34, 0xAB

#define BROADCAST_MAC 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
#define NULL_IP_ADDR 0x00, 0x00, 0x00, 0x00
#define BROADCAST_IP_ADDR 0xFF, 0xFF, 0xFF, 0xFF

/* MACRAW frame assembly components */
/* Ethernet header */
#define IPv4 0x08, 0x00
/* IPv4 header */
#define IPv4_INFO 0x45
#define DIFFSERV 0x00
#define IPv4_ID 0x84, 0x73
#define IPv4_FLAGS 0x00, 0x00
#define TTL 0x7D
#define PROTOCOL_UDP 0x11
/* UDP header */
#define UDP_SOURCE_PORT 0x00, 0x44
#define UDP_DEST_PORT 0x00, 0x43
#define FCS 0xDD, 0x6A, 0x0A, 0x9F

// Port numbers in DHCP message use
#define SERVER_PORT 67
#define CLIENT_PORT 68

/* Parts of the message header */
#define BOOTREQUEST 0x01
#define BOOTREPLY 0x02
#define HTYPE 0x01
#define HLEN 0x06
#define HOPS 0x00
#define FLAGS 0x00, 0x00
// An identifier of a given message chain, preferably randomised
#define XID 0x23, 0x87, 0x13, 0x65

/* Options */
#define MAGIC_COOKIE 0x63, 0x82, 0x53, 0x63
// DHCP message type identifier
#define MESSAGETYPE 0x35, 0x01
// Message types themselves
#define DISCOVER 0x01
#define ACQUIRED 0x11
#define FRESH_ACQUIRED 0x21
#define EXPIRED 0x31
#define OFFER 0x02
#define REQUEST 0x03
#define DECLINE 0x04
#define PACK 0x05
#define PNAK 0x06
// IP
#define REQUESTED_IP 0x32, 0x04, IP_ADDRESS
// The router and subnet mask of the current domain
#define DOMAIN_DATA 0x37, 0x02, 0x01, 0x03
// The server to which the request is addressed
#define SERVER 0x36, 0x04, 0x00, 0x00, 0x00, 0x00
// End of options (and message)
#define END 0xFF, 0x00

/* Offsets for different sections in header checksum calculations, from link layer/ethernet header start */
#define IPv4_LENGTH_STEP 2
#define IPv4_CHECKSUM_STEP 10
#define UDP_LENGTH_STEP 24
/* Offsets for different DHCP packet sections from link layer header start*/
#define UDP_SOURCE_STEP 26
#define YIADDR_STEP 58
#define YIADDR_TO_SIADDR_STEP 4
#define SIADDR_STEP 62
#define SIADDR_TO_ZEROES_STEP 24
#define CHADDR_STEP 70
#define CHADDR_TO_COOKIE_STEP 208
#define MAGIC_COOKIE_STEP 278
#define REQUESTED_IP_STEP 287
#define COOKIE_TO_MTYPE_STEP 6
#define COOKIE_TO_SERVER_STEP 15
#define COOKIE_TO_SUBNET_STEP 21
#define COOKIE_TO_GATEWAY_STEP 27
#define COOKIE_TO_LEASE_STEP 15

// For the DHCP tracker; how many iterations of the loop until a request is repeated
#define MESSAGE_DELAY 200000ul


/*  Holds data related to lease negotiations.
    Most things are stored in the W5500 itself to save memory (such as our and the server's IP addresses) */
typedef struct {
    // The phase of a DHCP negotiation we're in
    uint8_t dhcp_status;
    // The time since our last request
    uint32_t dhcp_lease_time;
    uint8_t server[4];
    uint8_t our_ip[4];
    uint8_t submask[4];
} DHCP_Client;

/* A single instance of DHCP Client for our use. */
extern DHCP_Client DHCP;
extern Socket DHCP_Socket;

void dhcp_setup();
void set_network();
/* A function to be put in the main loop that  */
void dhcp_tracker();
/* Interrupt handler for incoming messages */
void dhcp_interrupt();

/* Prints W5500's currently assigned IP */
void print_ip();