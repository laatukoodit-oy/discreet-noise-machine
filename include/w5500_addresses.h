#pragma once

/* Addresses from the common register block, first byte is irrelevant, only the last three matter
(bytes 1 and 2 are the address, last gets used as basis for a control frame) */
// Common block - Mode register
#define MR 0x00000000ul
// Common block - Gateway address IP register
#define GAR 0x00000100ul
// Common block - Subnet mask register
#define SUBR 0x00000500ul 
// Common block - Source hardware address register
#define SHAR 0x00000900ul 
// Common block - Source IP address
#define SIPR 0x00000F00ul
// Common block - Interrupt register
#define IR 0x00001500ul
// Common block - Interrupt mask register
#define IMR 0x00001600ul
// Common block - Socket interrupt register
#define SIR 0x00001700ul
// Common block - Socket interrupt mask register
#define SIMR 0x00001800ul
// Common block - PHY configuration register
#define PHYCFGR 0x00002E00ul
// Common block - Chip version register
#define VERSIONR 0x00003900ul

// Number of bytes in the registers above 
#define MR_LEN 1
#define GAR_LEN 4
#define SUBR_LEN 4
#define SHAR_LEN 6
#define SIPR_LEN 4
#define IR_LEN 1
#define IMR_LEN 1
#define SIR_LEN 1
#define SIMR_LEN 1
#define PHYCFGR_LEN 1
#define VERSIONR_LEN 1

/* Base addresses for sockets' registers (socket-no-based adjustment done later) */
// Socket's personal data
// Socket register block - Mode register
#define S_MR 0x00000008ul
// Socket register block - Command register
#define S_CR 0x00000108ul
// Socket register block - Interrupt register
#define S_IR 0x00000208ul
// Socket register block - Status register
#define S_SR 0x00000308ul
// Socket register block - Source port register
#define S_PORT 0x00000408ul 

// Counterparty data
// Socket register block - Destination hardware address register
#define S_DHAR 0x00000608ul
// Socket register block - Destination IP address register
#define S_DIPR 0x00000C08ul
// Socket register block - Destination port register
#define S_DPORT 0x00001008ul 
// Free space in the outgoing TX register
#define S_TX_FSR 0x00002008ul

// Socket register block - Free space in the outgoing TX buffer
#define S_TX_FSR 0x00002008ul
// Socket register block - TX buffer read pointer
#define S_TX_RD 0x00002208ul
// Socket register block - TX buffer write pointer
#define S_TX_WR 0x00002408ul
// Socket register block - RX buffer received size register
#define S_RX_RSR 0x00002608ul
// Socket register block - RX buffer read pointer
#define S_RX_RD 0x00002808ul
// Socket register block - RX buffer write pointer
#define S_RX_WR 0x00002A08ul

// Socket register block - Socket interrupt mask
#define S_IMR 0x00002C08ul

// Socket TX buffer
#define S_TX_BUF 0x00000010ul
// Socket RX buffer
#define S_RX_BUF 0x00000018ul

// Number of bytes in the registers above 
#define S_MR_LEN 1
#define S_CR_LEN 1
#define S_IR_LEN 1
#define S_SR_LEN 1
#define S_PORT_LEN 2
#define S_DHAR_LEN 6
#define S_DIPR_LEN 4
#define S_DPORT_LEN 2
#define S_TX_FSR_LEN 2
#define S_TX_RD_LEN 2
#define S_TX_WR_LEN 2
#define S_TX_RSR_LEN 2
#define S_RX_RD_LEN 2
#define S_RX_WR_LEN 2
#define S_IMR_LEN 1
