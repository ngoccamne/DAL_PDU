#ifndef TEST_UTIL_TWI_H_
#define TEST_UTIL_TWI_H_
#include <avr/io.h>
#define TW_STATUS (TWSR & 0xF8U)
#define TW_START 0x08U
#define TW_REP_START 0x10U
#define TW_MT_SLA_ACK 0x18U
#define TW_MT_DATA_ACK 0x28U
#define TW_MR_SLA_ACK 0x40U
#define TW_MR_DATA_ACK 0x50U
#define TW_MR_DATA_NACK 0x58U
#endif
