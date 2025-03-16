#ifndef SPI_H
#define SPI_H

/*
    A module for SPI communication between a W5500 and an Arduino (as per the pin assignments)
*/

#include <stdio.h>
#include <avr/io.h>

// TCP status codes
#define SOCK_CLOSED 0x00
#define SOCK_INIT 0x13
#define SOCK_LISTEN 0x14
#define SOCK_ESTABLISHED 0x17
#define SOCK_CLOSE_WAIT 0x1C 

typedef struct { 
    uint8_t sockno, status, connected_ip_address[4], connected_port[2];
    uint16_t portno;
} Socket;

typedef struct W5500_SPI_thing {
    // Pointer to any buffer used for data storage 
    uint8_t *spi_buf;
    uint16_t buf_len, buf_index;
    // Who we are
    uint8_t ip_address[4];
    // Nice bits of data about what sockets are in use
    Socket sockets[8];   

    void (*tcp_listen)(Socket socket);
    void (*tcp_get_connection_data)(Socket *socket);
    void (*tcp_send)(Socket socket, uint16_t messagelen, char message[]);
    void (*tcp_disconnect)(Socket socket);
    void (*tcp_close)(Socket socket);
} W5500_SPI;

// Setup of various addresses
W5500_SPI setup_w5500_spi(uint8_t *buffer, uint16_t buffer_len);

// Opens a port up for TCP Listen
void tcp_listen(Socket socket);
void tcp_get_connection_data(Socket *socket);
void tcp_send(Socket socket, uint16_t messagelen, char message[]);
void tcp_disconnect(Socket socket);
void tcp_close(Socket socket);

#endif