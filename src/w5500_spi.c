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

#define ENABLEINT0 (INT_ENABLE |= (1 << INT0))
#define DISABLEINT0 (INT_ENABLE &= (INT_ENABLE & ~(1 << INT0)))

#define MIN(a, b) ((a < b) ? a : b)

/* LOCAL*/
// What gets called when an interrupt comes in from the W5500
void (*w5500_interrupt)(int socketno, int interrupt);

// Helper for socket port assignment
void initialise_socket(Socket *socket, uint8_t socketno, uint16_t portno);
void enable_interrupts(uint8_t socketno);
void disable_interrupts(uint8_t socketno);

// Reads 2-byte value until it stabilises
uint16_t get_2_byte(uint32_t addr);

// Get data from W5500
void read(uint32_t addr, uint8_t *buffer, uint16_t bufferlen, uint16_t readlen);
// Write data to the W5500
void write(uint32_t addr, uint16_t datalen, uint8_t data[]);

// Sends header to start off transmission
void start_transmission(uint32_t addr);
// Chip select low
void end_transmission(void);

// 8 cycles of shifting a byte and setting an output port based on said bit
void writeByte(uint8_t data);

// Setting of various registers needed for INT0 interrupts
void setup_atthing_interrupts(void);


/* PUBLIC*/
// Device initialization
void setup_w5500_spi(W5500_SPI *w5500, uint8_t *buffer, uint16_t buffer_len, void (*interrupt_func)(int socketno, int interrupt)) {  
    // Clears the socket interrupt mask on the W5500
    uint8_t clear_interrupts = 0;
    write(SIMR, 1, &clear_interrupts);

    w5500->spi_buffer = buffer;
    w5500->buf_length = buffer_len;
    w5500->buf_index = 0;
    // IP address to 192.168.0.15 (for group 15)
    w5500->ip_address[0] = 0xC0; 
    w5500->ip_address[1] = 0xA8; 
    w5500->ip_address[2] = 0x00; 
    w5500->ip_address[3] = 0x0F;

    for (uint16_t i = 0; i < SOCKETNO; i++) {
        Socket newsock;
        initialise_socket(&newsock, i, 9999 + i);
        w5500->sockets[i] = newsock;

        disable_interrupts(i);
    }

    // Set pins to I or O as needed
    DDRB |= (1 << CLK) | (1 << SEL) | (1 << MO);
    DDRB &= ~(1 << MI);
    INTREG &= ~(1 << INTR);

    // Pins to 0, except for the select pin, which defaults to 1
    LOW(CLK);
    LOW(MO);
    HIGH(SEL);

    // Gateway address to 192.168.0.1
    uint8_t gw_addr[] = {0xC0, 0xA8, 0x00, 0x01};
    // Subnet mask to 255.255.255.0
    uint8_t sm_addr[] = {0xFF, 0xFF, 0xFF, 0x00};
    // Mac address to 4A-36-EE-E0-34-AB
    uint8_t mac_addr[] = {0x4A, 0x36, 0xEE, 0xE0, 0x34, 0xAB};

    // Set up the link as a 10M duplex connection
    // Feed in the new config and "use these bits for configuration" setting
    uint8_t config_option = (1 << OPMD) | (PHY_FD10BTNN << OPMDC);
    write(PHYCFGR, 1, &config_option);
    // Apply config
    uint8_t reset = (1 << RST);
    write(PHYCFGR, 1, &reset);

    // Set the various device addresses from above
    write(GAR, sizeof(gw_addr), gw_addr);
    write(SUBR, sizeof(sm_addr), sm_addr);
    write(SHAR, sizeof(mac_addr), mac_addr);
    write(SIPR, 4, w5500->ip_address);

    // Enable sending of interrupt signals on the W5500 and reading them here
    w5500_interrupt = interrupt_func;
    setup_atthing_interrupts();
}


/* TCP Connections */
// 
uint8_t tcp_listen(Socket *socket) {
    // Embed the socket number in the addresses to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket->sockno);
    uint32_t port_addr = S_PORT | socketmask;
    uint32_t mode_addr = S_MR | socketmask;
    uint32_t com_addr = S_CR | socketmask;
    uint32_t status_addr = S_SR | socketmask;
    // TCP commands
    uint8_t tcpmode = TCPMODE;
    uint8_t open = OPEN;
    uint8_t listen = LISTEN;

    // Assign port to socket
    uint8_t port[] = {(socket->portno >> 8), socket->portno};
    write(port_addr, sizeof(port), port);

    // Set socket mode to TCP
    write(mode_addr, 1, &tcpmode);
    // Open socket
    write(com_addr, 1, &open);
    // Ensure that the socket is ready to take a listen command
    uint8_t status[] = {0};
    uint8_t killswitch = 0;
    do {
        read(status_addr, status, 1, S_SR_LEN);
        killswitch++;
        if (killswitch > 100) {
            return 1;
        }
    } while (status[0] != SOCK_INIT);

    // Get the socket listening
    write(com_addr, 1, &listen);

    // Enable interrupts for the socket
    enable_interrupts(socket->sockno);

    return 0;
}

void tcp_get_connection_data(Socket *socket) {
    // Embed the socket number in the address to hit the right register block
    uint8_t socketmask = (socket->sockno << 5);
    uint32_t status_addr = S_SR | socketmask;

    read(status_addr, &socket->status, 1, S_SR_LEN);

    if (socket->status == SOCK_ESTABLISHED) {
        uint32_t ip_addr = S_DIPR | socketmask;
        uint32_t port_addr = S_DPORT | socketmask;
        read(ip_addr, socket->connected_ip_address, 4, S_DIPR_LEN);
        read(port_addr, socket->connected_port, 2, S_DPORT_LEN);
    }
}

void tcp_send(Socket *socket, uint16_t messagelen, char message[]) {
    
    // Embed the socket number in the addresses to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket->sockno);
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

void tcp_read_received(W5500_SPI *w5500, Socket *socket) {
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

    uint16_t read_amount = MIN(w5500->buf_length, received_amount);

    read(rx_addr, w5500->spi_buffer, w5500->buf_length, read_amount);

    // Update W55000's buffer read index
    w5500->buf_index = read_amount;

    // Update read pointer
    uint8_t new_rx_pointer[] = {(read_amount + rx_pointer) >> 8, (read_amount + rx_pointer)};
    write(rx_pointer_addr, sizeof(new_rx_pointer), new_rx_pointer);
    uint8_t recv = RECV;
    write(recv_addr, 1, &recv);
}

void tcp_disconnect(Socket *socket) {
    // Embed the socket number in the address to hit the right register block
    uint8_t socketmask = SOCKETMASK(socket->sockno);
    uint32_t com_addr = S_CR | socketmask;

    uint8_t discon = DISCON;
    write(com_addr, 1, &discon);

    disable_interrupts(socket->sockno);
}

void tcp_close(Socket *socket) {
    // Embed the socket number in the address to hit right register block
    uint8_t socketmask = SOCKETMASK(socket->sockno);
    uint32_t com_addr = S_CR | socketmask;

    uint8_t close = CLOSE;
    write(com_addr, 1, &close);

    disable_interrupts(socket->sockno);
}


/* LOCAL*/
/* Socket operations */
void initialise_socket(Socket *socket, uint8_t socketno, uint16_t portno) {
    socket->sockno = socketno;
    socket->portno = portno;
    socket->status = 0;
}

void enable_interrupts(uint8_t socketno) {
    uint32_t gen_interrupt_mask_addr = SIMR;
    uint32_t sock_interrupt_mask_addr = S_IMR | SOCKETMASK(socketno);

    // Enable interrupts for the pin
    // Get existing register contents
    uint8_t simr[] = {0};
    read(gen_interrupt_mask_addr, simr, 1, SIMR_LEN);
    // Embed socket number into it
    simr[0] |= (1 << socketno);
    write(gen_interrupt_mask_addr, SIMR_LEN, simr);

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
    uint8_t simr[] = {0};
    read(gen_interrupt_mask_addr, simr, 1, SIMR_LEN);
    // Remove socket number from it
    simr[0] &= ~(1 << socketno);
    write(gen_interrupt_mask_addr, SIMR_LEN, simr);

    // Disable all interrupts for the socket 
    uint8_t interrupts = 0;
    write(sock_interrupt_mask_addr, 1, &interrupts);
}


/* Buffer manipulation */
void clear_spi_buffer(W5500_SPI *w5500) { 
    for (uint16_t i = 0; i < w5500->buf_index; i++) {
        w5500->spi_buffer[i] = 0;
    }
    w5500->buf_index = 0;
}

#ifndef __AVR_Atiny85__
void print_buffer(uint8_t *buffer, uint16_t bufferlen, uint16_t printlen) {
    printf("Buffer contents: 0x%X", buffer[0]);
    uint16_t len = (printlen <= bufferlen ? printlen : bufferlen);
    for (uint16_t i = 1; i < len; i++) {  
        printf("-%X", buffer[i]);
    }
    printf("\r\n");
}
#endif

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
    // Don't let a reading operation get interrupted by INT0, as that contains other
    // reads and writes that would mess with the chip select and message contents
    DISABLEINT0;
    
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

    ENABLEINT0;
}

// Write to W5500's register(s), number of bytes written given by datalen (should be sizeof(data))
void write(uint32_t addr, uint16_t datalen, uint8_t data[]) {
    // Don't let a writing operation get interrupted by INT0, as that contains other
    // reads and writes that would mess with the chip select and message contents
    DISABLEINT0;
    
    // Set write bit in header frame
    uint32_t address = addr | (1 << 2);
    // Send header
    start_transmission(address);

    // Uses writeByte to push message through the pipeline byte by byte
    for (uint16_t i = 0; i < datalen; i++) {
        writeByte(data[i]);
    }

    end_transmission();

    ENABLEINT0;
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

void setup_atthing_interrupts(void) {
    cli();

    // Set INT0 to trogger on low
    INT_MODE = (INT_MODE & ~((1 << ISC00) | (1 << ISC01)));
    // Enable INT0
    ENABLEINT0;
    // Enable interrupts in general in the status register
    SREG |= (1 << SREG_I);
}

ISR(INT0_vect) {
    // 
    uint32_t general_interrupt_addr = SIR;
    uint32_t socket_interrupt_addr = S_IR;
    uint8_t gen = 0, sock = 0, clearmsg = 0;

    // Check which socket is alerting
    read(general_interrupt_addr, &gen, 1, SIR_LEN);
    for (uint8_t i = 0; i < SOCKETNO; i++) {
        if (EXTRACTBIT(gen, i)) {
            // Get the socket's interrupt register to see what's going on
            sock = 0;
            socket_interrupt_addr = EMBEDSOCKET(socket_interrupt_addr, i);
            read(socket_interrupt_addr, &sock, 1, S_IR_LEN);
            // The interruptmask is used to set which interrupts are active, so any extras can be ignored
            sock &= INTERRUPTMASK;
            if (sock > 0) {
                // There are five possible interrupts
                for (uint8_t j = 0; j < 5; j++) {
                    if (EXTRACTBIT(sock, j) > 0) {
                        // Clear that interrupt bit by writing a 1 to it
                        clearmsg = (1 << j);
                        write(socket_interrupt_addr, 1, &clearmsg);

                        (*w5500_interrupt)(i, j);
                    } 
                }
            }
            read(socket_interrupt_addr, &sock, 1, S_IR_LEN);
        }
    }
}

