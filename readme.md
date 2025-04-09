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

## w5500.c and .h user's guide

A set of files for communication between a W5500 network device and an ATtiny85 or ATmega328P. Provides an instance of a W5500 interface by the name Wizchip, and functions for controlling sockets on the W5500 device and transmitting data back and forth over an ethernet connection.

Below, "Wizchip" refers to the interface struct included in w5500.c, and "W5500" to the physical device used for network transmissions.

---

### How to use?

Have a buffer for data storage and an interrupt function to be called when a connection or message arrives. Set up Wizchip with the function and buffer. Use the functions (explained below in more detail) to set up sockets for TCP connections and transfer data back and forth between the socket(s) and end users.

```c
#include "w5500.h"

uint8_t buffer[BUFFERSIZE] = {0, };

int main(void) {
    // Assign buffer and function
    setup_wizchip(buffer, BUFFERSIZE, &interruptfunction);
}

void interruptfunction(int socket, uint8_t interrupts) {
    // Received a message, read it
    if (interrupts & (1 << RECV_INT)) {
        tcp_read_received(&Wizchip.sockets[socket]);
    }
}
```

---

### Data (structures)

#### Macros 

- SOCKETNO, used to set the number of sockets in use (max. 8)
- Socket TCP status codes, format SOCK_[code]
- Interrupt bit offsets, format [interrupt]_INT

#### Structs

- Struct Socket, contains
    - The socket's number
    - The socket's status code (refreshed with tcp_get_connection_data())
    - The connected party's IP address
    - The connected party's port number
- Struct W5500, contains
    - A pointer to a buffer used for data transfer
    - The buffer's size 
    - An index indicating how much of the buffer was filled by the last read
    - The device's assigned IP address
    - An array of sockets, sized [SOCKETNO]

---

### Callables

#### void setup_wizchip(uint8_t *buffer, uint8_t buffer_len, void (*interrupt_func)(int socketno, uint8_t interrupt))

Sets up Wizchip with network addresses, config settings, and port numbers for its sockets. Also enables INT0 interrupts on the microcontroller for the purposes of receiving interrupts from the W5500.

Takes:
    
- Wizchip's address
- The address and size of a buffer of your choice. This buffer is where data received from the W5500 is deposited.
- A function to be called when an interrupt comes in from the W5500. The function takes the socket that the interrupt comes from (0 to SOCKETNO - 1) and a byte containing the interrupt bits currently active. The structure of the byte is 

    > 0b [-]  [-]  [-]  [SENDOK]  [TIMEOUT]  [RECV]  [DISCON]  [CON].

    Offsets for accessing each bit are included in the header, and come in the format "[interrupt]_INT". Currently the TIMEOUT interrupt is disabled due to excessive frequency and irrelevance.

---
#### uint8_t tcp_listen(Socket *socket)

Sets up the given socket for TCP Listen mode. Sends in commands to open the socket and then set it up as a listener. Includes checks where the status of the device is polled to make sure it is ready to proceed to the next step. These checks may return early with an error if the setup doesn't proceed in a timely manner.

Takes: 

- A pointer to the socket to be set up

Returns:

- 0 on successful initialization
- A status code read from the socket on unsuccessful initialization. Some codes are included in w5500.h with a "SOCK_" prefix, the rest are available in the data sheet. A code not found in the data sheet suggests a data transfer/read issue. 

---
#### void tcp_get_connection_data(Socket *socket)

Polls the W5500 for its status and fetches a connected party's data if a TCP connection is ongoing. The fetched data is stored in the socket's status, connected_ip_address and connected_port variables, from which they can be read afterwards.

Takes: 

- A pointer to the socket to be checked

---
#### bool tcp_send(Socket *socket, uint8_t message_len, char message[], bool progmem, bool sendnow)

Writes a given message to the socket's TX buffer from where it will be sent out to the connected party.

Takes:

- A pointer to the socket used for transmitting the message
- The length of the message to be sent
- A pointer to the message
- A boolean indicating whether the message is stored in program memory
- A boolean indicating whether the message should be sent out at the end of the function call. Allows the user to insert a larger message into the TX buffer in multiple small chunks.

Returns: 

- 0 on success
- 1 if there isn't enough space in the TX buffer to hold the whole message