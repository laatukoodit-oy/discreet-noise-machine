#ifndef SPI_H
#define SPI_H

/*
    A module for SPI communication between a W5500 and an Arduino (as per the pin assignments)
*/

#include <stdio.h>
#include <avr/io.h>

typedef struct {
    // 
    int sockno, portno, mode;
} Socket;

typedef struct W5500_SPI_thing {
    // Pointer to a buffer used for data storage 
    uint8_t *spi_buf;
    int buf_len, buf_index;
    // Who we are
    uint8_t ip_address[4];
    // Nice bits of data about what sockets are in use
    Socket sockets[8];   

    void (*tcp_listen)(struct W5500_SPI_thing *w5500, Socket socket);
} W5500_SPI;

// Setup of various addresses
W5500_SPI setup_w5500_spi(uint8_t *buffer, int buffer_len);

// Opens a port up for TCP Listen
void tcp_listen(W5500_SPI *w5500, Socket socket);

// Get register data from W5500
void read(W5500_SPI *w5500, long addr, int readlen);
// Write data to register(s) of W5500
void write(W5500_SPI *w5500, long addr, int datalen, uint8_t data[]);

#endif