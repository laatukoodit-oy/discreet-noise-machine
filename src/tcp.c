#include "tcp.h"

/* 
    TCP
*/

// 
uint8_t tcp_listen(const Socket *socket) {
    //uart_write_P(PSTR("TCP listen.\r\n"));

    // Embed the socket number in the addresses to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket->sockno);
    uint32_t com_addr = S_CR | socketmask;
    uint32_t status_addr = S_SR | socketmask;

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

/* Writes a message to the socket's TX buffer and sends in the "send" command.
- Sending a 0 through send_now allows you to postpone the sending of register contents, 
meaning you can fill up the buffer in small chunks. 
- Progmem = 1 if you're sending in a pointer to an array in program memory rather than a normal array */
uint8_t tcp_send(const Socket *socket, uint8_t message_len, const char *message, bool progmem, bool send_now) {
    uart_write_P(PSTR("TCP send.\r\n"));
    
    // Embed the socket number in the addresses to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket->sockno);
    uint32_t tx_free_addr = S_TX_FSR | socketmask;
    uint32_t tx_pointer_addr = S_TX_WR | socketmask;
    uint32_t tx_addr = S_TX_BUF | socketmask;
    uint32_t send_addr = S_CR | socketmask;

    // Check space left in the buffer (shouldn't run out but you never know)
    uint16_t send_amount = get_2_byte(tx_free_addr);
    if (send_amount < message_len) {
        return 1;
    }
  
    // Get the TX buffer write pointer, adjust address to start writing from that point
    uint16_t tx_pointer = get_2_byte(tx_pointer_addr);
    tx_addr = EMBEDADDRESS(tx_addr, tx_pointer);
    
    // Write message to buffer
    if (progmem) {
        write_P(tx_addr, message_len, message);
    } else {
        write(tx_addr, message_len, message);
    }

    // Write a new value to the TX buffer write pointer to match post-input situation
    uint8_t new_tx_pointer[] = {(message_len + tx_pointer) >> 8, (message_len + tx_pointer)};
    write(tx_pointer_addr, S_TX_WR_LEN, new_tx_pointer);

    // Send the "send" command to pass the message to the other party
    if (send_now) {
        uint8_t send = SEND;
        write(send_addr, 1, &send);
    }

    return 0;
}

/* Reads the socket's RX buffer into a given buffer
- As the only goal for our server is to get the path from a message, the message is 
marked as entirely received even when only a small amount is in fact read */
void tcp_read_received(const Socket *socket, uint8_t *buffer, uint8_t buffer_len) {
    uart_write_P(PSTR("TCP read received.\r\n"));
    
    // Embed the socket number in the address to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket->sockno);
    uint32_t rx_length_addr = S_RX_RSR | socketmask;
    uint32_t rx_pointer_addr = S_RX_RD | socketmask;
    uint32_t rx_addr = S_RX_BUF | socketmask;
    uint32_t recv_addr = S_CR | socketmask;

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

void tcp_disconnect(const Socket *socket) {
    uart_write_P(PSTR("TCP disconnect.\r\n"));
    
    // Embed the socket number in the address to hit the right register block
    uint32_t com_addr = S_CR | SOCKETMASK(socket->sockno);

    uint8_t discon = DISCON;
    write(com_addr, 1, &discon);
}

void tcp_close(const Socket *socket) {
    uart_write_P(PSTR("TCP close.\r\n"));
    
    socket_close(socket);
}