#pragma once


#define COMMON_BLOCK 0x00
#define SOCKET_BLOCK 0x08
// Socket TX buffer block address
#define S_TX_BUF_BLOCK 0x10
// Socket RX buffer block address
#define S_RX_BUF_BLOCK 0x18

/* Addresses from the common register block, first byte is irrelevant, only the last three matter
(bytes 1 and 2 are the address, last gets used as basis for a control frame) */
// Common block - Mode register
#define MR_B 0x00
#define MR 0x00, 0x00, COMMON_BLOCK
// Common block - Gateway address IP register
#define GAR_B 0x01
#define GAR 0x00, 0x01, COMMON_BLOCK
// Common block - Subnet mask register
#define SUBR_B 0x05
#define SUBR 0x00, 0x05, COMMON_BLOCK
// Common block - Source hardware address register
#define SHAR_B 0x09
#define SHAR 0x00, 0x09, COMMON_BLOCK
// Common block - Source IP address
#define SIPR_B 0x0F
#define SIPR 0x00, 0x0F, COMMON_BLOCK
// Common block - Interrupt register
#define IR_B 0x15
#define IR 0x00, 0x15, COMMON_BLOCK
// Common block - Interrupt mask register
#define IMR_B 0x16
#define IMR 0x00, 0x16, COMMON_BLOCK
// Common block - Socket interrupt register
#define SIR_B 0x17
#define SIR 0x00, 0x17, COMMON_BLOCK
// Common block - Socket interrupt mask register
#define SIMR_B 0x18
#define SIMR 0x00, 0x18, COMMON_BLOCK
// Common block - PHY configuration register
#define PHYCFGR_B 0x2E
#define PHYCFGR 0x00, 0x2E, COMMON_BLOCK
// Common block - Chip version register
#define VERSIONR_B 0x39
#define VERSIONR 0x00, 0x39, COMMON_BLOCK

/* Base addresses for sockets' registers (socket-no-based adjustment done later) */
// Sockets' personal data
// Socket register block - Mode register
#define S_MR_B 0x00
#define S_MR 0x00, 0x00, SOCKET_BLOCK
// Socket register block - Command register
#define S_CR_B 0x01
#define S_CR 0x00, 0x01, SOCKET_BLOCK
// Socket register block - Interrupt register
#define S_IR_B 0x02
#define S_IR 0x00, 0x02, SOCKET_BLOCK
// Socket register block - Status register
#define S_SR_B 0x03
#define S_SR 0x00, 0x03, SOCKET_BLOCK
// Socket register block - Source port register
#define S_PORT_B 0x04
#define S_PORT 0x00, 0x04, SOCKET_BLOCK 

// Counterparty data
// Socket register block - Destination hardware address register
#define S_DHAR_B 0x06
#define S_DHAR 0x00, 0x06, SOCKET_BLOCK
// Socket register block - Destination IP address register
#define S_DIPR_B 0x0C
#define S_DIPR 0x00, 0x0C, SOCKET_BLOCK
// Socket register block - Destination port register
#define S_DPORT_B 0x10
#define S_DPORT 0x00, 0x10, SOCKET_BLOCK 
// Free space in the outgoing TX register
#define S_TX_FSR_B 0x20
#define S_TX_FSR 0x00, 0x20, SOCKET_BLOCK
// Socket register block - TX buffer read pointer
#define S_TX_RD_B 0x22
#define S_TX_RD 0x00, 0x22, SOCKET_BLOCK
// Socket register block - TX buffer write pointer
#define S_TX_WR_B 0x24
#define S_TX_WR 0x00, 0x24, SOCKET_BLOCK
// Socket register block - RX buffer received size register
#define S_RX_RSR_B 0x26
#define S_RX_RSR 0x00, 0x26, SOCKET_BLOCK
// Socket register block - RX buffer read pointer
#define S_RX_RD_B 0x28
#define S_RX_RD 0x00, 0x28, SOCKET_BLOCK
// Socket register block - RX buffer write pointer
#define S_RX_WR_B 0x2A
#define S_RX_WR 0x00, 0x2A, SOCKET_BLOCK
// Socket register block - Socket interrupt mask
#define S_IMR_B 0x2C
#define S_IMR 0x00, 0x2C, SOCKET_BLOCK

