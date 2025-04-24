/*
    Socket controls for W5500.
*/

#include "socket.h"


/* Attaches port number, interrupt mask and mode of operation to socket */
void socket_initialise(Socket *socket, uint8_t mode, uint16_t portno, uint8_t interrupt_mask) {
    uint32_t port_addr = S_PORT | SOCKETMASK(socket->sockno);
    uint32_t mode_addr = S_MR | SOCKETMASK(socket->sockno);
    uint32_t tx_wr_addr = S_TX_WR | SOCKETMASK(socket->sockno);

    // Assign port to socket, write portno into the socket's registers
    socket->portno = portno;
    uint8_t port[] = {(portno >> 8), portno};
    write(port_addr, 2, port);

    // Set socket mode (TCP, UDP or MACRAW)
    socket->mode = mode;
    write(mode_addr, 1, &socket->mode);

    // Assign interrupt mask to socket, write to socket
    socket->interrupts = interrupt_mask;
    socket_toggle_interrupts(socket, 0);

    // Gets the socket's current TX buffer write pointer, takes note for future writes
    socket->tx_pointer = get_2_byte(tx_wr_addr);
}

/* Opens socket, updates struct's tx write pointer to match the current read pointer as that gets initialised with socket opens */
void socket_open(Socket *socket) {
    uint32_t com_addr = S_CR | SOCKETMASK(socket->sockno);
    uint32_t read_pointer_addr = S_TX_RD | SOCKETMASK(socket->sockno);

    // Open socket
    uint8_t command = OPEN;
    write(com_addr, 1, &command);

    if (socket->mode != TCP_MODE) {
        socket_toggle_interrupts(socket, 1);
    }

    // 
    socket->tx_pointer = get_2_byte(read_pointer_addr);
}

/* Self-explanatory. */
void socket_close(const Socket *socket) {
    uint32_t com_addr = S_CR | SOCKETMASK(socket->sockno);

    uint8_t close = CLOSE;
    write(com_addr, 1, &close);

    socket_toggle_interrupts(socket, 0);
}

/* Reads the socket's status register into the socket struct */
void socket_get_status(Socket *socket) {
    uint32_t status_addr = S_SR | SOCKETMASK(socket->sockno);

    read(status_addr, &socket->status, 1, S_SR_LEN);
}

/*  Toggles a socket's interrupts on or off based on the socket's interrupt mask. 
    Will always include a CON_INT for TCP sockets for pointer tracking purposes, even if not requested by user. */
void socket_toggle_interrupts(const Socket *socket, bool set_on) {
    uint32_t gen_interrupt_mask_addr = SIMR;
    uint32_t sock_interrupt_mask_addr = S_IMR | SOCKETMASK(socket->sockno);

    // Get existing socket interrupt mask register
    uint8_t simr = 0;
    read(gen_interrupt_mask_addr, &simr, 1, SIMR_LEN);

    // Embed socket number into it, write
    simr &= ~_BV(socket->sockno);
    simr |= (set_on << socket->sockno);
    write(gen_interrupt_mask_addr, 1, &simr);

    // TCP connection events affect buffer pointers and need to be accounted for in the ISR
    // regardless of whether the end user cares about them
    uint8_t interrupts = ((socket->mode == TCP_MODE) ? (socket->interrupts | CON_INT) : socket->interrupts);

    // Send a list of accepted interrupts for the socket
    write(sock_interrupt_mask_addr, 1, &interrupts);
}

/* Updates the socket's TX write pointer register and commands the W5500 to transmit the contents of socket TX buffer. */
void socket_send_message(Socket *socket) {
    uint32_t tx_pointer_addr = S_TX_WR | SOCKETMASK(socket->sockno);
    uint32_t send_addr = S_CR | SOCKETMASK(socket->sockno);

    // Write a new value to the TX buffer write pointer to match post-input situation
    uint8_t new_tx_write_pointer[2] = {(socket->tx_pointer >> 8), socket->tx_pointer};
    write(tx_pointer_addr, 2, new_tx_write_pointer);

    // Send the "send" command to pass the message to the other party
    uint8_t send = (socket->mode == MACRAW_MODE ? SEND_MAC : SEND);
    write(send_addr, 1, &send);
}