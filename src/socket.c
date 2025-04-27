/*
    Socket controls for W5500.
*/

#include "socket.h"


void embed_pointer(uint16_t pointer) {
    wizchip_address[0] = (pointer >> 8);
    wizchip_address[1] = pointer;
}

void set_address(uint8_t byte_one, uint8_t byte_two, uint8_t block) {
    wizchip_address[0] = byte_one;
    wizchip_address[1] = byte_two;
    wizchip_address[2] = block;
}

/* Attaches port number, interrupt mask and mode of operation to socket */
void socket_initialise(Socket *socket, uint8_t mode, uint16_t portno, uint8_t interrupt_mask) {
    // Assign port to socket, write portno into the socket's registers
    socket->portno = portno;
    uint8_t port[] = {(portno >> 8), portno};
    set_address(S_PORT);
    embed_socket(socket->sockno);
    write(2, port);

    // Set socket mode (TCP, UDP or MACRAW)
    socket->mode = mode;
    set_half_address(S_MR_B);
    write(1, &socket->mode);

    // Assign interrupt mask to socket, write to socket
    socket->interrupts = interrupt_mask;
    socket_toggle_interrupts(socket, OFF);

    // Gets the socket's current TX buffer write pointer, takes note for future writes
    set_half_address(S_TX_WR_B);
    socket->tx_pointer = get_2_byte();
}

/* Opens socket, updates struct's tx write pointer to match the current read pointer as that gets initialised with socket opens */
void socket_open(Socket *socket) {
    // Open socket
    uint8_t command = OPEN;
    set_address(S_CR);
    embed_socket(socket->sockno);
    write(1, &command);

    if (socket->mode != TCP_MODE) {
        socket_toggle_interrupts(socket, ON);
    }

    //
    set_half_address(S_TX_RD_B);
    socket->tx_pointer = get_2_byte();
}

/* Self-explanatory. */
void socket_close(const Socket *socket) {
    uint8_t close = CLOSE;
    set_address(S_CR);
    embed_socket(socket->sockno);
    write(1, &close);

    socket_toggle_interrupts(socket, OFF);
}

/* Reads the socket's status register into the socket struct */
void socket_get_status(Socket *socket) {
    set_address(S_SR);
    embed_socket(socket->sockno);
    read(&socket->status, 1, 1);
}

/*  Toggles a socket's interrupts on or off based on the socket's interrupt mask.
    Will always include a CON_INT for TCP sockets for pointer tracking purposes, even if not requested by user. */
void socket_toggle_interrupts(const Socket *socket, bool set_on) {
    // Get existing socket interrupt mask register
    uint8_t simr = 0;
    set_address(SIMR);
    embed_socket(socket->sockno);
    read(&simr, 1, 1);

    // Embed socket number into it, write
    simr &= ~_BV(socket->sockno);
    simr |= (set_on << socket->sockno);
    write(1, &simr);

    // TCP connection events affect buffer pointers and need to be accounted for in the ISR
    // regardless of whether the end user cares about them
    uint8_t interrupts = ((socket->mode == TCP_MODE) ? (socket->interrupts | CON_INT) : socket->interrupts);

    // Send a list of accepted interrupts for the socket
    set_address(S_IMR);
    embed_socket(socket->sockno);
    write(1, &interrupts);
}

void socket_update_read_pointer(const Socket *socket, uint16_t read_pointer) {
    uint8_t pointer[] = {(read_pointer >> 8), read_pointer};

    set_address(S_RX_RD);
    embed_socket(socket->sockno);
    write(2, pointer);

    uint8_t recv = RECV;
    set_half_address(S_CR_B);
    write(1, &recv);
}

/* Updates the socket's TX write pointer register and commands the W5500 to transmit the contents of socket TX buffer. */
void socket_send_message(const Socket *socket) {
    // Write a new value to the TX buffer write pointer to match post-input situation
    uint8_t new_tx_write_pointer[2] = {(socket->tx_pointer >> 8), socket->tx_pointer};
    set_address(S_TX_WR);
    embed_socket(socket->sockno);
    write(2, new_tx_write_pointer);

    // Send the "send" command to pass the message to the other party
    uint8_t send = (socket->mode == MACRAW_MODE ? SEND_MAC : SEND);
    set_half_address(S_CR_B);
    write(1, &send);
}