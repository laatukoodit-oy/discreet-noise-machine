/*
    A module for ethernet communication by an ATmega328P or ATtiny85 with a W5500
*/

#pragma once


#include <avr/interrupt.h>
#include <stdbool.h>

#include "dhcp.h"
#include "tcp.h"


/* User-relevant macros below */

// Number of sockets available for use in the Wizchip (max. 7 as one is taken by the DHCP client) 
// (see SOCKETNO below)
#define USER_SOCKETNO 1

/* User-relevant macros above */


/* Device settings */
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


// Number of sockets in use (so as to not use more memory to hold them than necessary).
// W5500 has 8 in total.
#define SOCKETNO (USER_SOCKETNO + DHCP_SOCKETNO)
#if SOCKETNO > 8 
    #error "Too many sockets requested"
#endif

// The last socket in the list is reserved for DHCP use
#define DHCP_SOCKET (SOCKETNO - 1)

// The number of interrupts that can be stored before new ones get dropped
#define INTERRUPT_LIST_SIZE 5


/* Device structure */
typedef struct {
    // Who we are
    DHCP_Client *dhcp;
    // Nice bits of data about what sockets are in use
    Socket sockets[SOCKETNO];
    // A list of interrupts that have arrived from W5500
    uint8_t interrupt_list[INTERRUPT_LIST_SIZE];
    volatile uint8_t interrupt_list_index;
} W5500;


// The module provides a single W5500 instance to the user
extern W5500 Wizchip;


// Device initialization
void setup_wizchip(void);