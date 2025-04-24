/*
    Module for TCP communication using a W5500.
*/

#pragma once

#include "socket.h"


// TCP status codes
#define SOCK_CLOSED 0x00
#define SOCK_INIT 0x13
#define SOCK_LISTEN 0x14
#define SOCK_ESTABLISHED 0x17
#define SOCK_CLOSE_WAIT 0x1C 

// TCP socket control commands
#define LISTEN 0x02
#define CONNECT 0x04
#define DISCON 0x08

// Different bits in the tcp_send operand.
#define OP_PROGMEM 0x01
#define OP_HOLDBACK 0x02


/* Basic setup to get the socket ready for operation. */
void tcp_socket_initialise(Socket *socket, uint16_t portno, uint8_t interrupts);
/*  Puts the socket into TCP listen mode. 
    If the socket setup doesn't proceed as expected, returns the status code of the socket. */ 
uint8_t tcp_listen(Socket *socket);
/*  Writes a message to the socket's TX buffer and sends in the "send" command.
    Operands: 
    - OP_PROGMEM if you're sending in a pointer to an array in program memory rather than a normal array 
    - OP_HOLDBACK if you want to delay sending the message and write more into the buffer */
uint8_t tcp_send(Socket *socket, uint16_t message_len, const char *message, uint8_t operands);
/*  Reads the socket's RX buffer into a given buffer
    - As the only goal for our server is to get the path from a message, the message is 
    marked as entirely received even when only a small amount is in fact read */
void tcp_read_received(const Socket *socket, uint8_t *buffer, uint8_t buffer_len);
/* Sends a disconnect command to the socket, which will start a connection close process. */
void tcp_disconnect(const Socket *socket);
/* Closes the socket. */
void tcp_close(const Socket *socket);