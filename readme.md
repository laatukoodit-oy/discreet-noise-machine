# Title

## Usage

The project `Makefile` expects a `make.conf` in the project root directory with the following variables:

- PROGRAMMER
- BAUD\_RATE
- PORT
- MCU
- F\_CPU

An example make.conf for an Arduino UNO board on port COM4:

```make
PROGRAMMER = arduino
BAUD_RATE = 115200
PORT = COM4

MCU = atmega328p
F_CPU = 16000000UL
```

---
---

## w5500.c and .h (and tcp.c and .h) user's guide

A set of files for communication between a W5500 network device and an ATtiny85 or ATmega328P. Provides an instance of a W5500 interface by the name Wizchip, and functions for controlling sockets on the W5500 device and transmitting data back and forth over an ethernet connection.

Includes a DHCP client for IP address acquisition, [which doesn't work yet so this paragraph ends here].

Below, "Wizchip" refers to the interface struct included in w5500.c, and "W5500" to the physical device used for network transmissions.

---

### How to use?

Set USER_SOCKETNO in W5500.h to suit your needs (1 to 7 sockets, which will start from index 0). Set up Wizchip with setup_wizchip() and poll for interrupts continuously using check_interrupts(). Alter check_interrupts() to suit your needs (such as by reacting to new connections by sending back a webpage). Use the functions from tcp.c (explained below in more detail) to set up sockets for TCP connections and transfer data back and forth between the socket(s) and end users. 

```c
#include "w5500.h"

// A basic HTTP reply
const char content[] PROGMEM = "HTTP/1.1 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><body><h1>Test</h1></body></html>";

void check_interrupts();
void shuffle_interrupts();

int main(void) {
    setup_wizchip();

    // Get socket 0 listening on port 9999
    tcp_initialise_socket(&Wizchip.sockets[0], 9999, CON_INT);
    tcp_listen(&Wizchip.sockets[0]);

    for (;;) {    
        check_interrupts();
    }

    return 0;
}

void check_interrupts() {
    // Check the list for a new interrupt
    if (Wizchip.interrupt_list_index == 0) {
        return;
    }

    // Extract the socket number from the list
    uint8_t interrupt = Wizchip.interrupt_list[0];
    uint8_t sockno = interrupt >> 5;

    // DHCP operations, do not touch
    if (sockno == DHCP_SOCKET) {
        dhcp_interrupt(&Wizchip.sockets[DHCP_SOCKET], interrupt);
        shuffle_interrupts();
        return;
    }

    /* User code below */

    // When connected, send a response and close the connection
    if (interrupt & CON_INT) {
        tcp_send(&Wizchip.sockets[sockno], sizeof(content), content, OP_PROGMEM);
        tcp_disconnect(&Wizchip.sockets[sockno]);
    }

    /* User code above */

    shuffle_interrupts();
}

void shuffle_interrupts() {
    cli();
    // Shuffle everything along down the list
    for (int i = 0; i < Wizchip.interrupt_list_index; i++) {
        Wizchip.interrupt_list[i] = Wizchip.interrupt_list[i + 1];
    }
    Wizchip.interrupt_list[Wizchip.interrupt_list_index] = 0;
    Wizchip.interrupt_list_index--;
    sei();
}
```

---

### Data (structures)

Wizchip - An instance of W5500 for use by you, the user.

#### Macros 

- USER_SOCKETNO, located in w5500.h and used to set the number of sockets in use.  Can be altered to enable more sockets (max. 7, as one is taken by the DHCP client).
- Socket TCP status codes, format SOCK_[code]
- Interrupt bit offsets, format [interrupt]_INT
- Send operands for tcp_send(), format OP_[option]

#### Structs

- Struct Socket, contains
    - sockno - The socket's number
    - status - The socket's status code (refreshed with socket_get_status())
    - mode - The socket's mode (TCP for TCP use)
    - interrupts - The interrupts to be received
    - portno - The socket's port number
    - tx_pointer - Tracks the socket's TX buffer's wrte pointer, as the read value of the pointer doesn't update simply from writing to it
- Struct W5500, contains
    - *dhcp - A pointer to the DHCP client
    - sockets[] - A list of sockets
    - interrupt_list[] - A list for holding incoming interrupts before they're processed
    - interrupt_list_index - An index for tracking how much of the interrupt list is filled

---

### Callables

#### void setup_wizchip(void)

Sets up Wizchip with network addresses, config settings, and port numbers for its sockets. Also enables INT0 interrupts on the microcontroller for the purposes of receiving interrupts from the W5500, and starts up the DHCP client.

---

#### void tcp_initialise_socket(Socket *socket, uint16_t portno, uint8_t interrupts)

Initialises the socket in TCP mode, feeding in the given port number and setting it up to alert with the given interrupts. Doesn't open the socket or set it up to listen.

Takes: 

- *socket - A pointer to the socket to be set up
- portno - A port number to associate with the socket
- interrupts - A byte with the interrupts that you want alerts for
    - SENDOK_INT activates when a message is successfully sent
    - TIMEOUT_INT activates on a TCP timeout event
    - RECV_INT activates whenever you receive something in the RX buffer
    - DISCON_INT activates on successful closure of a connection
    - CON_INT activates when a connection is established

---

#### uint8_t tcp_listen(Socket *socket)

Sets up the given socket for TCP Listen mode. Sends in commands to open the socket and then set it up as a listener. Includes checks where the status of the device is polled to make sure it is ready to proceed to the next step. These checks may return early with an error if the setup doesn't proceed in a timely manner.

Takes: 

- *socket - A pointer to the socket to be set up

Returns:

- 0 on successful initialization*
- A status code read from the socket on unsuccessful initialization*. Some codes are included in w5500.h with a "SOCK_" prefix, the rest are available in the data sheet. A code not found in the data sheet suggests a data transfer/read issue. 

    <small>* Code 0x00 signifies a closed socket, which unfortunately overlaps with a successful return.</small>

---

#### uint8_t tcp_send(Socket *socket, uint16_t message_len, const char *message, uint8_t operands)

Writes a given message to the socket's TX buffer from which it will be sent out to the connected party. Can 

Takes:

- *socket - A pointer to the socket used for transmitting the message
- message_len - The length of the message to be sent
- *message - A pointer to the message
- operands - A byte containing bit flags for different operations to be performed.
    - OP_PROGMEM if the array you're sending in is located in program memory
    - OP_HOLDBACK to not send the message after writing. Allows you to write a message into the buffer in multiple pieces before sending it out at once.

Returns: 

- 0 on success
- 1 if there isn't enough space in the TX buffer to hold the whole message

---

#### void tcp_read_received(const Socket *socket, uint8_t *buffer, uint8_t buffer_len)

Reads the contents of a socket's RX buffer into a given buffer of your own. As our limited-capability server doesn't care about things like HTTP headers, the entire message is marked as read even if a smaller amount is actually accessed from the RX buffer.

Takes:

- *socket - A pointer to the socket that you want to read from
- *buffer - A pointer to the buffer used to hold the read data
- buffer_len - The length of the read buffer

---

#### void tcp_disconnect(const Socket *socket)

The socket will perform a TCP connection termination operation.

Takes:

- *socket - A pointer to the socket

---

#### void tcp_close(const Socket *socket)

Closes the socket.

Takes:

- *socket - A pointer to the socket