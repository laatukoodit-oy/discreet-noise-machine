/*
    A module for SPI communication between a W5500 and either an ATmega328P or ATtiny85
*/

#include "w5500_spi.h"

// Macros for things too small to be a function yet too weird to read
#define LOW(pin) PORTB &= ~(1 << pin)
#define HIGH(pin) PORTB |= (1 << pin)
#define WRITEOUTPUT(out) PORTB = (PORTB & ~(1 << MO)) | (out << MO)
#define READINPUT ((PINB & (1 << MI)) >> MI)

#define EXTRACTBIT(byte, index) (byte & (1 << index)) >> index

#define SOCKETMASK(sockno) (sockno << 5)
#define EMBEDSOCKET(base_addr, sockno) (base_addr & 0xFFFFFF1F) | (sockno << 5)
#define EMBEDADDRESS(base_addr, new_addr) (base_addr & 0xFF0000FF) | ((uint32_t)new_addr << 8)

#define MIN(a, b) ((a < b) ? a : b)

/* LOCAL*/
// What gets called when an interrupt comes in from the W5500
void (*w5500_interrupt)(int socketno, int interrupt);

// Helper for socket port assignment
void initialise_socket(Socket *socket, uint8_t socketno, uint16_t portno);
void enable_interrupts(uint8_t socketno);
void disable_interrupts(uint8_t socketno);

void clear_spi_buffer(W5500_SPI *w5500);
void print_buffer(uint8_t *buffer, uint16_t bufferlen, uint16_t printlen);

// Reads 2-byte value until it stabilises
uint16_t get_2_byte(uint32_t addr);

// Get data from W5500
void read(uint32_t addr, uint8_t *buffer, uint16_t bufferlen, uint16_t readlen);
// Write data to the W5500
void write(uint32_t addr, uint16_t datalen, uint8_t data[]);

// Sends header to start of transmission
void start_transmission(uint32_t addr);
// Chip select low
void end_transmission(void);

// 8 cycles of shifting a byte and setting a output port based on said bit
void writeByte(uint8_t data);

void setup_atmega_interrupts(void);


/* PUBLIC*/
// Setup of various addresses
W5500_SPI setup_w5500_spi(uint8_t *buffer, uint16_t buffer_len, void (*interrupt_func)(int socketno, int interrupt)) {  
    printf("W5500 setup started.\n");

    uint32_t gen_interrupt_mask_addr = SIMR;
    uint8_t clear_interrupts[] = {0};
    write(gen_interrupt_mask_addr, 1, clear_interrupts);

    // Gateway address to 192.168.0.1
    uint8_t gw_addr[] = {0xC0, 0xA8, 0x00, 0x01};
    // Subnet mask to 255.255.255.0
    uint8_t sm_addr[] = {0xFF, 0xFF, 0xFF, 0x00};
    // Mac address to 4A-36-EE-E0-34-AB
    uint8_t mac_addr[] = {0x4A, 0x36, 0xEE, 0xE0, 0x34, 0xAB};
    // IP address to 192.168.0.15 (for group 15)
    uint8_t ip_addr[] = {0xC0, 0xA8, 0x00, 0x0F};

    Socket sockets[8];
    for (uint16_t i = 0; i < 8; i++) {
        Socket newsock;
        initialise_socket(&newsock, i, 9999 + i);
        sockets[i] = newsock;

        disable_interrupts(i);
    }

    W5500_SPI w5500 = {
        .spi_buffer = buffer,
        .buf_length = buffer_len,
        .buf_index = 0,
        .ip_address = {*ip_addr},
        .sockets = {*sockets},
        
        .tcp_listen = &tcp_listen,
        .tcp_get_connection_data = &tcp_get_connection_data,
        .tcp_read_received = &tcp_read_received,
        .tcp_send = &tcp_send,
        .tcp_close = &tcp_close,
        .tcp_disconnect = &tcp_disconnect,
    };

    // Set pins to I or O as needed
    DDRB &= ~(1 << MI);
    DDRB |= (1 << CLK) | (1 << SEL) | (1 << MO);

    // Set INT0's pin as input for interrupts coming from the W5500
    INTREG &= ~(1 << INTR);

    // Pins to 0, except for the select pin, which defaults to 1
    LOW(CLK);
    LOW(MO);
    HIGH(SEL);
 
    // Set the various device addresses from above
    write(GAR, sizeof(gw_addr), gw_addr);
    write(SUBR, sizeof(sm_addr), sm_addr);
    write(SHAR, sizeof(mac_addr), mac_addr);
    write(SIPR, sizeof(ip_addr), ip_addr);

    // Enable sending of interrupt signals on the W5500 and reading them here
    w5500_interrupt = interrupt_func;
    setup_atmega_interrupts();

    return w5500;
}


/* TCP Connections */
// 
void tcp_listen(Socket socket) {
    // Embed the socket number in the addresses to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket.sockno);
    uint32_t port_addr = S_PORT | socketmask;
    uint32_t mode_addr = S_MR | socketmask;
    uint32_t com_addr = S_CR | socketmask;
    uint8_t tcpmode = TCPMODE;
    uint8_t open = OPEN;
    uint8_t listen = LISTEN;

    printf("Opening port %d for socket %d, mode TCP listen.\n", socket.portno, socket.sockno);

    // Assign port to socket
    uint8_t port[] = {(socket.portno >> 8), socket.portno};
    write(port_addr, S_PORT_LEN, port);

    // Enable interrupts for the socket
    enable_interrupts(socket.sockno);

    // Set socket mode to TCP
    write(mode_addr, S_MR_LEN, &tcpmode);
    // Open socket
    write(com_addr, S_CR_LEN, &open);
    // Get the socket listening
    write(com_addr, S_CR_LEN, &listen);
    
    // Check status
    tcp_get_connection_data(&socket);
}

void tcp_get_connection_data(Socket *socket) {
    // Embed the socket number in the address to hit the right register block
    uint8_t socketmask = (socket->sockno << 5);
    uint32_t status_addr = S_SR | socketmask;

    read(status_addr, &socket->status, 1, S_SR_LEN);

    printf("Connection data for socket %d: status %d", socket->sockno, socket->status);

    if (socket->status == SOCK_ESTABLISHED) {
        uint32_t ip_addr = S_DIPR | socketmask;
        uint32_t port_addr = S_DPORT | socketmask;
        read(ip_addr, socket->connected_ip_address, 4, S_DIPR_LEN);
        read(port_addr, socket->connected_port, 2, S_DPORT_LEN);
        printf(", connected to IP %u.%u.%u.%u and port %d", socket->connected_ip_address[0], socket->connected_ip_address[1], socket->connected_ip_address[2], socket->connected_ip_address[3], (socket->connected_port[0] + socket->connected_port[1]));
    }

    printf(".\n");
}

void tcp_send(Socket socket, uint16_t messagelen, char message[]) {
    
    // Embed the socket number in the addresses to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket.sockno);
    uint32_t tx_free_addr = S_TX_FSR | socketmask;
    uint32_t tx_pointer_addr = S_TX_WR | socketmask;
    uint32_t tx_addr = S_TX_BUF | socketmask;
    uint32_t send_addr = S_CR | socketmask;

    // If the whole message can't fit in the register, send it in multiple pieces
    uint16_t message_left = messagelen;    
    do {
        // Get the TX buffer write pointer, adjust address to start writing from that point
        uint16_t tx_pointer = get_2_byte(tx_pointer_addr);
        tx_addr = EMBEDADDRESS(tx_addr, tx_pointer);

        uint16_t send_amount = get_2_byte(tx_free_addr);
        
        // Compare free space to the remaining message, send as much as you can
        send_amount = MIN(send_amount, message_left);
        write(tx_addr, send_amount, &message[messagelen - message_left]);
        message_left -= send_amount;

        // Write a new value to the TX buffer write pointer to match post-input situation
        uint8_t new_tx_pointer[] = {(send_amount + tx_pointer) >> 8, (send_amount + tx_pointer)};
        write(tx_pointer_addr, S_TX_WR_LEN, new_tx_pointer);
        
        // Send the "send" command to pass the message to the other party
        uint8_t send = SEND;
        write(send_addr, 1, &send);
        
    } while (message_left > 0);
}

void tcp_read_received(W5500_SPI w5500, Socket socket) {
    // Embed the socket number in the address to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket.sockno);
    uint32_t rx_length_addr = S_RX_RSR | socketmask;
    uint32_t rx_pointer_addr = S_RX_RD | socketmask;
    uint32_t rx_addr = S_RX_BUF | socketmask;
    uint32_t recv_addr = S_CR | socketmask;

    // Check how much has come in and where you left off reading
    uint16_t received_amount = get_2_byte(rx_length_addr);
    uint16_t rx_pointer = get_2_byte(rx_pointer_addr);
    rx_addr = EMBEDADDRESS(rx_addr, rx_pointer);

    uint16_t read_amount = MIN(w5500.buf_length, received_amount);

    read(rx_addr, w5500.spi_buffer, w5500.buf_length, read_amount);

    // Update read pointer
    uint8_t new_rx_pointer[] = {(read_amount + rx_pointer) >> 8, (read_amount + rx_pointer)};
    write(rx_pointer_addr, S_RX_RD_LEN, new_rx_pointer);
    uint8_t recv = RECV;
    write(recv_addr, 1, &recv);
}

void tcp_disconnect(Socket socket) {
    // Embed the socket number in the address to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket.sockno);
    uint32_t com_addr = S_CR | socketmask;

    printf("Closing TCP connection in socket %d.\n", socket.sockno);

    uint8_t discon = DISCON;
    write(com_addr, 1, &discon);    
}

void tcp_close(Socket socket) {
    // Embed the socket number in the address to hit right register block
    uint8_t socketmask = SOCKETMASK(socket.sockno);
    uint32_t com_addr = S_CR | socketmask;

    printf("Closing socket %d.\n", socket.sockno);

    uint8_t close = CLOSE;
    write(com_addr, 1, &close);

    disable_interrupts(socket.sockno);
}


/* LOCAL*/
/* Socket operations */
void initialise_socket(Socket *socket, uint8_t socketno, uint16_t portno) {
    socket->sockno = socketno;
    socket->portno = portno;
    socket->status = 0;
}

void enable_interrupts(uint8_t socketno) {
    printf("Enabling interrupts\n");
    
    uint32_t gen_interrupt_mask_addr = SIMR;
    uint32_t sock_interrupt_mask_addr = S_IMR | SOCKETMASK(socketno);

    // Enable interrupts for the pin
    // Get existing register contents
    uint8_t simr = 0;
    read(gen_interrupt_mask_addr, &simr, 1, SIMR_LEN);
    // Embed socket number into it
    simr |= (1 << socketno);
    write(gen_interrupt_mask_addr, SIMR_LEN, &simr);

    // Enable most interrupts for the socket 
    uint8_t interrupts = INTERRUPTMASK;
    //(1 << CON_INT) | (1 << DISCON_INT) | (1 << RECV_INT) | (1 << SENDOK_INT);
    write(sock_interrupt_mask_addr, 1, &interrupts);
}

void disable_interrupts(uint8_t socketno) {
    uint32_t gen_interrupt_mask_addr = SIMR;
    uint32_t sock_interrupt_mask_addr = S_IMR | SOCKETMASK(socketno);

    // Disable interrupts for the pin
    // Get existing register contents
    uint8_t simr = 0;
    read(gen_interrupt_mask_addr, &simr, 1, SIMR_LEN);
    // Remove socket number from it
    simr &= ~(1 << socketno);
    write(gen_interrupt_mask_addr, SIMR_LEN, &simr);

    // Disable all interrupts for the socket 
    uint8_t interrupts = 0;
    write(sock_interrupt_mask_addr, 1, &interrupts);
}


/* Buffer manipulation */
void clear_spi_buffer(W5500_SPI *w5500) { 
    printf("Clearing buffer.\n");
    for (uint16_t i = 0; i < w5500->buf_index; i++) {
        w5500->spi_buffer[i] = 0;
    }
    w5500->buf_index = 0;
}

void print_buffer(uint8_t *buffer, uint16_t bufferlen, uint16_t printlen) {
    printf("Buffer contents: 0x%X", buffer[0]);
    uint16_t len = (printlen <= bufferlen ? printlen : bufferlen);
    for (uint16_t i = 1; i < len; i++) {
        printf("-%X", buffer[i]);
    }
    printf("\n");
}

// As 2-byte register values may change during reading, check them until subsequent reads match
uint16_t get_2_byte(uint32_t addr) {
    uint8_t read1[] = {0, 0}, read2[] = {0, 0}; 
    do {
        read2[0] = read1[0];
        read2[1] = read1[1];
        read(addr, read1, 2, 2);
    } while (read1[0] != read2[0] || read1[1] != read2[1]);
    return ((uint16_t)read2[0] << 8 | read2[1]);
}


/* Transmissions */
// Conducts a read operation fetching information from register(s), number of fetched bytes given by readlen
void read(uint32_t addr, uint8_t *buffer, uint16_t bufferlen, uint16_t readlen) {
    printf("Reading from address %lX.\n", addr);
    
    // Send header
    start_transmission(addr);

    uint8_t byte = 0;

    // Check read length against available buffer size, cap if necessary
    uint16_t len = (readlen <= bufferlen ? readlen : bufferlen);
    // (could use some sort of error if not all can be read)
    
    for (uint16_t i = 0; i < len; i++) {
        // Reads MISO line, fills byte bit by bit
        byte = 0;
        for (int j = 0; j < 8; j++) {
            LOW(CLK);
            byte = (byte << 1) + READINPUT;
            HIGH(CLK);
        }
        buffer[(uint8_t)i] = byte;
    }
    end_transmission();

    // Push buffer contents out for visibility
    print_buffer(buffer, bufferlen, readlen);
}

// Write to W5500's register(s), number of bytes written given by datalen (should be sizeof(data))
void write(uint32_t addr, uint16_t datalen, uint8_t data[]) {
    // Set write bit in header frame
    uint32_t address = addr | (1 << 2);
    // Send header
    start_transmission(address);
    
    printf("Sending data to address %lX: ", address);
    // Uses writeByte to push message through the pipeline byte by byte
    for (uint16_t i = 0; i < datalen; i++) {
        printf("%X-", data[i]);

        writeByte(data[i]);
    }
    printf("Data sent.\n");

    end_transmission();
}

void start_transmission(uint32_t addr) {
    // Chip select low to initiate transmission
    LOW(SEL);
    
    // HEADER:
    // The first byte of the 4-byte header is irrelevant
    writeByte((uint8_t)(addr >> 16));
    writeByte((uint8_t)(addr >> 8));
    writeByte((uint8_t)addr);

    // Header sent, set output pin to low
    LOW(MO);
}

void end_transmission() {
    // Chip select high to end transmission
    HIGH(SEL);
}

// Shift bits from MSB to LSB to the MOSI pipe, cycle clock to send
void writeByte(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        LOW(CLK);
        WRITEOUTPUT(EXTRACTBIT(data, i));
        HIGH(CLK);
    }
}

void setup_atmega_interrupts() {
    cli();

    #if defined(__AVR_ATtiny85__)
    printf("ATtiny interrupt config.\n");  
    #elif defined(__AVR_ATmega328P__)
    printf("ATmega interrupt config.\n");
    #else 
    printf("Other MCU interrupt config.\n");
    #endif

    // Enable interrupts in general in the status register
    SREG |= (1 << SREG_I);
    // Trigger mode for INT0 to "low", aka zeros
    INT0_TRIGGER_MODE &= ~((1 << ISC00) | (1 << ISC01));
    // Enable reading of INT0 interrupts
    INT_ENABLE_MASK |= (1 << INT0);
        
    sei();
}

ISR(INT0_vect) {
    // Check for source of interrupt
    printf("Interrupt triggered.\n");

    uint32_t general_interrupt_addr = SIR;
    uint32_t socket_interrupt_addr = S_IR;
    uint8_t gen = 0, sock = 0;

    // Check which socket is alerting
    read(general_interrupt_addr, &gen, 1, SIR_LEN);
    for (uint8_t i = 0; i < 8; i++) {
        if (EXTRACTBIT(gen, i)) {
            sock = 0;
            socket_interrupt_addr = EMBEDSOCKET(socket_interrupt_addr, i);
            read(socket_interrupt_addr, &sock, 1, S_IR_LEN);
            sock &= INTERRUPTMASK;
            if (sock > 0) {
                for (uint8_t j = 0; j < 5; j++) {
                    if (EXTRACTBIT(sock, j) > 0) {
                        // Clear that interrupt bit
                        uint8_t clearmsg = (1 << j);
                        write(socket_interrupt_addr, 1, &clearmsg);

                        (*w5500_interrupt)(i, j);
                    } 
                }
            }
        }
    }
}

