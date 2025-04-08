#include "socket.h"


/* 
    Socket operations 
*/
uint8_t initialise_socket(Socket *socket, uint8_t mode, uint16_t portno, uint8_t interrupt_mask) {
    // Embed the socket number in the addresses to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket->sockno);
    uint32_t port_addr = S_PORT | socketmask;
    uint32_t mode_addr = S_MR | socketmask;

    // Assign port to socket
    socket->portno = portno;
    uint8_t port[] = {(portno >> 8), portno};
    write(port_addr, 2, port);

    // Set socket mode
    socket->mode = mode;
    write(mode_addr, 1, &socket->mode);

    // Assign interrupt mask to socket, write to socket
    socket->interrupts = interrupt_mask;
    toggle_socket_interrupts(socket, 0);

    return 0;
}

void socket_open(const Socket *socket) {
    // Embed the socket number in the addresses to hit the right register block
    uint32_t com_addr = S_CR | SOCKETMASK(socket->sockno);

    // Open socket
    uint8_t command = OPEN;
    write(com_addr, 1, &command);
}

void socket_close(const Socket *socket) {
    uart_write_P(PSTR("Socket close.\r\n"));
    
    // Embed the socket number in the address to hit right register block
    uint32_t com_addr = S_CR | SOCKETMASK(socket->sockno);

    uint8_t close = CLOSE;
    write(com_addr, 1, &close);

    toggle_socket_interrupts(socket, 0);
}

void socket_get_status(Socket *socket) {
    // Embed the socket number in the address to hit the right register block
    uint8_t socketmask = (socket->sockno << 5);
    uint32_t status_addr = S_SR | socketmask;

    read(status_addr, &socket->status, 1, S_SR_LEN);
}


void toggle_socket_interrupts(const Socket *socket, bool set_on) {
    //uart_write_P(PSTR("Toggle interrupts.\r\n"));
    
    uint32_t gen_interrupt_mask_addr = SIMR;
    uint32_t sock_interrupt_mask_addr = S_IMR | SOCKETMASK(socket->sockno);

    // Get existing socket interrupt mask register
    uint8_t simr = 0;
    read(gen_interrupt_mask_addr, &simr, 1, SIMR_LEN);

    // Embed socket number into it
    simr &= ~(1 << socket->sockno);
    simr |= (set_on << socket->sockno);
    write(gen_interrupt_mask_addr, 1, &simr);

    // Send a list of accepted interrupts for the socket
    write(sock_interrupt_mask_addr, 1, &socket->interrupts);
}