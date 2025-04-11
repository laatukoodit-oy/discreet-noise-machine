/*
    Module for TCP communication using a W5500.
*/

#include "tcp.h"


/* Basic setup to get the socket ready for operation. */
void tcp_initialise_socket(Socket *socket, uint16_t portno, uint8_t interrupts) {
    initialise_socket(socket, TCP_MODE, portno, interrupts);
}

/*  Puts the socket into TCP listen mode. 
    If the socket setup doesn't proceed as expected, returns the status code of the socket. */ 
uint8_t tcp_listen(Socket *socket) {
    uart_write_P(PSTR("TCP listen.\r\n"));

    uint32_t com_addr = S_CR | SOCKETMASK(socket->sockno);
    uint32_t status_addr = S_SR | SOCKETMASK(socket->sockno);

    socket_open(socket);

    // Ensure that the socket is ready to take a listen command
    uint8_t status = 0;
    uint8_t killswitch = 0;
    do {
        read(status_addr, &status, 1, S_SR_LEN);
        killswitch++;
        if (killswitch > 100) {
            return status;
        }
    } while (status != SOCK_INIT);

    // Get the socket listening
    uint8_t command = LISTEN;
    write(com_addr, 1, &command);

    // Make sure the socket is in fact listening
    killswitch = 0;
    do {
        read(status_addr, &status, 1, S_SR_LEN);
        killswitch++;
        if (killswitch > 100) {
            return status;
        }
    // SOCK_ESTABLISHED included in case there's been an immediate connection
    } while (status != SOCK_LISTEN && status != SOCK_ESTABLISHED);
    
    // Enable interrupts for the socket
    toggle_socket_interrupts(socket, 1);

    return 0;
}

/*  Writes a message to the socket's TX buffer and sends in the "send" command.
    Operands: 
    - OP_PROGMEM if you're sending in a pointer to an array in program memory rather than a normal array 
    - OP_HOLDBACK if you want to delay sending the message and write more into the buffer */
uint8_t tcp_send(Socket *socket, uint16_t message_len, const char *message, uint8_t operands) {
    uart_write_P(PSTR("TCP send.\r\n"));
    
    uint32_t tx_free_addr = S_TX_FSR | SOCKETMASK(socket->sockno);
    uint32_t tx_addr = S_TX_BUF | SOCKETMASK(socket->sockno);

    // Check space left in the buffer (shouldn't run out but you never know)
    uint16_t send_amount = get_2_byte(tx_free_addr);
    if (send_amount < message_len) {
        return 1;
    }
  
    // Start writing from the place you left off
    tx_addr = EMBEDADDRESS(tx_addr, socket->tx_pointer);
    
    // Write message to buffer
    if (operands & OP_PROGMEM) {
        write_P(tx_addr, message_len, message);
    } else {
        write(tx_addr, message_len, message);
    }

    // Increment write pointer
    socket->tx_pointer += message_len;

    // Don't send the message yet if HOLDBACK is active
    if (operands & OP_HOLDBACK) {
        return 0;
    }

    send_send_command(socket);

    return 0;
}

/*  Reads the socket's RX buffer into a given buffer
    - As the only goal for our server is to get the path from a message, the message is 
    marked as entirely received even when only a small amount is in fact read */
void tcp_read_received(const Socket *socket, uint8_t *buffer, uint8_t buffer_len) {
    uart_write_P(PSTR("TCP read received.\r\n"));

    uint32_t rx_length_addr = S_RX_RSR | SOCKETMASK(socket->sockno);
    uint32_t rx_pointer_addr = S_RX_RD | SOCKETMASK(socket->sockno);
    uint32_t rx_addr = S_RX_BUF | SOCKETMASK(socket->sockno);
    uint32_t recv_addr = S_CR | SOCKETMASK(socket->sockno);

    // Check how much has come in and where you left off reading
    uint16_t received_amount = get_2_byte(rx_length_addr);
    uint16_t rx_pointer = get_2_byte(rx_pointer_addr);
    rx_addr = EMBEDADDRESS(rx_addr, rx_pointer);

    uint16_t read_amount = MIN(buffer_len, received_amount);

    // Read incoming into the buffer
    read(rx_addr, buffer, buffer_len, read_amount);

    // Update read pointer to mark everything as read
    uint8_t new_rx_pointer[] = {(received_amount + rx_pointer) >> 8, (received_amount + rx_pointer)};
    write(rx_pointer_addr, sizeof(new_rx_pointer), new_rx_pointer);
    uint8_t recv = RECV;
    write(recv_addr, 1, &recv);
}

/* Sends a disconnect command to the socket, which will start a connection close process. */
void tcp_disconnect(const Socket *socket) {    
    uart_write_P(PSTR("TCP disconnect.\r\n"));
    uint32_t com_addr = S_CR | SOCKETMASK(socket->sockno);

    uint8_t discon = DISCON;
    write(com_addr, 1, &discon);
}

/* Closes the socket. */
void tcp_close(const Socket *socket) {
    uart_write_P(PSTR("TCP close.\r\n"));
    
    socket_close(socket);
}