#pragma once

#include "socket.h"

/* TCP */
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

/* TCP */
// Opens a port up for TCP Listen
uint8_t tcp_listen(const Socket *socket);
uint8_t tcp_send(const Socket *socket, uint16_t message_len, const char *message, bool progmem, bool sendnow);
void tcp_read_received(const Socket *socket, uint8_t *buffer, uint8_t buffer_len);
void tcp_disconnect(const Socket *socket);
void tcp_close(const Socket *socket);
