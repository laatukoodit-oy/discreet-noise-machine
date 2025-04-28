/*
    Module for TCP communication using a W5500.
*/

#include "tcp.h"

Socket TCP_Socket;

/* Basic setup to get the socket ready for operation. */
void tcp_socket_initialise(uint16_t portno, uint8_t interrupts) {
    socket_initialise(&TCP_Socket, TCP_MODE, portno, interrupts);
}

/*  Puts the socket into TCP listen mode.
    If the socket setup doesn't proceed as expected, returns the status code of the socket. */
uint8_t tcp_listen() {
    socket_open(&TCP_Socket);

    // Ensure that the socket is ready to take a listen command
    uint8_t status = 0;
    uint8_t killswitch = 0;
    set_half_address(S_SR_B);
    do {
        read(&status, 1, 1);
        killswitch++;
        if (killswitch > 100) {
            return status;
        }
    } while (status != SOCK_INIT);

    // Get the socket listening
    set_half_address(S_CR_B);
    uint8_t command = LISTEN;
    write(1, &command);

    // Make sure the socket is in fact listening
    killswitch = 0;
    set_half_address(S_SR_B);
    do {
        read(&status, 1, 1);
        killswitch++;
        if (killswitch > 100) {
            return status;
        }
    // SOCK_ESTABLISHED included in case there's been an immediate connection
    } while (status != SOCK_LISTEN && status != SOCK_ESTABLISHED);

    // Enable interrupts for the socket
    socket_toggle_interrupts(&TCP_Socket, ON);

    return 0;
}

/*  Writes a message to the socket's TX buffer and sends in the "send" command.
    Operands:
    - OP_PROGMEM if you're sending in a pointer to an array in program memory rather than a normal array
    - OP_HOLDBACK if you want to delay sending the message and write more into the buffer */
uint8_t tcp_send(uint16_t message_len, const char *message, uint8_t operands) {
    // Check space left in the buffer (shouldn't run out but you never know)
    set_address(S_TX_FSR);
    embed_socket(1);
    uint16_t send_amount = get_2_byte();
    if (send_amount < message_len) {
        return 1;
    }

    // Start writing from the place you left off
    set_address((TCP_Socket.tx_pointer >> 8), TCP_Socket.tx_pointer, S_TX_BUF_BLOCK);
    embed_socket(1);

    // Write message to buffer
    if (operands & OP_PROGMEM) {
        write_P(message_len, message);
    } else {
        write(message_len, message);
    }

    // Increment write pointer
    TCP_Socket.tx_pointer += message_len;

    // Don't send the message yet if HOLDBACK is active
    if (operands & OP_HOLDBACK) {
        return 0;
    }

    socket_send_message(&TCP_Socket);

    return 0;
}

/*  Reads the socket's RX buffer into a given buffer
    - As the only goal for our server is to get the path from a message, the message is
    marked as entirely received even when only a small amount is in fact read */
void tcp_read_received(uint8_t *buffer, uint8_t buffer_len) {
    // Check how much has come in and where you left off reading
    set_address(S_RX_RSR);
    embed_socket(1);
    uint16_t received_amount = get_2_byte();
    set_half_address(S_RX_RD_B);
    uint16_t rx_pointer = get_2_byte();

    uint16_t read_amount = MIN(buffer_len, received_amount);

    // Read incoming into the buffer
    set_address((rx_pointer >> 8), rx_pointer, S_RX_BUF_BLOCK);
    embed_socket(1);

    read(buffer, buffer_len, read_amount);

    rx_pointer += received_amount;

    // Update the read pointer
    socket_update_read_pointer(&TCP_Socket, rx_pointer);
}

/* Sends a disconnect command to the socket, which will start a connection close process. */
void tcp_disconnect() {
    set_address(S_CR);
    embed_socket(1);

    uint8_t discon = DISCON;
    write(1, &discon);
}

/* Closes the socket. */
void tcp_close() {
    socket_close(&TCP_Socket);
}