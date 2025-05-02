# The Auditory Nuisance Machine

A device with a web server on an 8-bit microcontroller and integrated speaker for auditory distraction purposes.

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

## w5500.c and .h (and tcp, dhcp) user's guide

A set of files for communication between a W5500 network device and an ATtiny85 or ATmega328P. Provides an instance of a W5500 interface by the name Wizchip, and functions for controlling an assigned TCP socket on the W5500 device and transmitting data back and forth over an ethernet connection.

Includes a DHCP client for IP address acquisition. The client will negotiate an IP for the device from a nearby DHCP server and output the IP through the UART line once one has been acquired.

Below, "Wizchip" refers to the interface struct included in w5500.c, and "W5500" to the physical device used for network transmissions.

---

### How to use?

**Wizchip in general:** Set up Wizchip with setup_wizchip() and poll for interrupts continuously using check_interrupts(). Alter check_interrupts() to suit your needs (such as by reacting to new connections by sending back a webpage).

**TCP:** Use the functions from tcp.c/.h (explained below in more detail) to set up the TCP socket and transfer data back and forth between the socket and end users.

**DHCP:** Basic DHCP initialisation is performed in setup_wizchip(). dhcp_tracker() takes care of sending DHCP messages and acts as a "timer" for address renewal, and should be continuously polled, such as alongside check_interrupts(). In check_interrupts() dhcp_interrupt() should be called whenever an interrupt comes in for socket 0 (DHCP_SOCKET).

```c
#include "w5500.h"

// A basic HTTP reply
const char content[] PROGMEM = "HTTP/1.1 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><body><h1>Test</h1></body></html>";

void check_interrupts();
void shuffle_interrupts();

int main(void) {
    setup_wizchip();

    // Get socket 0 listening on port 9999
    tcp_socket_initialise(9999, (RECV_INT | DISCON_INT));
    tcp_listen();

    for (;;) {
        dhcp_tracker();
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
        dhcp_interrupt();
        shuffle_interrupts();
        return;
    }

    /* User code below */

    // When connected, send a response and close the connection
    if (interrupt & CON_INT) {
        tcp_send(sizeof(content), content, OP_PROGMEM);
        tcp_disconnect();
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

- Wizchip - An instance of W5500 for use by you, the user.
- TCP_Socket - The socket used for TCP communication
- DHCP_Socket - The socket used by the DHCP client

#### Macros

- DHCP socket number (0), DHCP_SOCKET
- Socket TCP status codes, format SOCK_[code]
- Interrupt bit offsets, format [interrupt]_INT
- Send operands for tcp_send(), format OP_[option]

#### Structs

- Struct Socket, contains
    - sockno - The socket's number
    - status - The socket's status code (refreshed with socket_get_status())
    - mode - The socket's mode (TCP for TCP use, MACRAW for DHCP)
    - interrupts - The interrupts to be received
    - portno - The socket's port number
    - tx_pointer - Tracks the socket's TX buffer's wrte pointer, as the read value of the pointer doesn't update simply from writing to it
- Struct W5500, contains
    - sockets[] - A list of sockets
    - interrupt_list[] - A list for holding incoming interrupts before they're processed
    - interrupt_list_index - An index for tracking how much of the interrupt list is filled

---

### Callables

#### void setup_wizchip(void)

Sets up W5500 with config settings. Also enables INT0 interrupts on the microcontroller for the purposes of receiving interrupts from the W5500, and starts up the DHCP client.

---

#### void tcp_initialise_socket(uint16_t portno, uint8_t interrupts)

Initialises the allocated TCP socket in TCP mode, feeding in the given port number and setting it up to alert with the given interrupts. Doesn't yet open the socket or set it up to listen.

Takes:

- portno - A port number to associate with the socket
- interrupts - A byte with the interrupt bit flags that you want alerts for
    - SENDOK_INT activates when a message is successfully sent
    - TIMEOUT_INT activates on a TCP timeout event
    - RECV_INT activates whenever you receive something in the RX buffer
    - DISCON_INT activates on successful closure of a connection
    - CON_INT activates when a connection is established

---

#### uint8_t tcp_listen()

Sets up the socket for TCP Listen mode. Sends in commands to open the socket and then set it up as a listener. Includes checks where the status of the device is polled to make sure it is ready to proceed to the next step. These checks may return early with an error if the setup doesn't proceed in a timely manner.

Returns:

- 0 on successful initialization*
- A status code read from the socket on unsuccessful initialization*. Some codes are included in w5500.h with a "SOCK_" prefix, the rest are available in the data sheet. A code not found in the data sheet suggests a data transfer/read issue.

    <small>* Code 0x00 signifies a closed socket, which unfortunately overlaps with a successful return.</small>

---

#### uint8_t tcp_send(uint16_t message_len, const char *message, uint8_t operands)

Writes a given message to the socket's TX buffer, from which it will be sent out to the connected party. Can access data in program memory or delay the sending of the message with bitflags in the operands field.

Takes:

- message_len - The length of the message to be sent
- *message - A pointer to the message
- operands - A byte containing bit flags for different operations to be performed.
    - OP_PROGMEM if the array you're sending in is located in program memory
    - OP_HOLDBACK to not send the message after writing. Allows you to write a message into the buffer in multiple pieces before sending it out all at once.

Returns:

- 0 on success
- 1 if there isn't enough space in the TX buffer to hold the whole message

---

#### void tcp_read_received(uint8_t *buffer, uint8_t buffer_len)

Reads the contents of the socket's RX buffer into a given buffer of your own. As our limited-capability server doesn't care about things like HTTP headers, the entire message is marked as read even if only a smaller amount is actually accessed from the RX buffer.

Takes:

- *buffer - A pointer to the buffer used to hold the read data
- buffer_len - The length of the read buffer

---

#### void tcp_disconnect()

The socket will perform a TCP connection termination operation.

---

#### void tcp_close()

Closes the socket.
