#ifndef AVR_I2C_H_
#define AVR_I2C_H_

#include <stdbool.h>
#include <stdint.h>

bool I2C_AVR_Init(uint32_t cpu_hz, uint32_t bus_hz);
bool I2C_AVR_Start(uint8_t address_rw);
bool I2C_AVR_Write(uint8_t value);
bool I2C_AVR_Read(uint8_t *value, bool ack);
void I2C_AVR_Stop(void);
/* A transaction that ran out of wait time sets the timeout flag. A NACK does not. */
void I2C_AVR_ClearTimeout(void);
bool I2C_AVR_TimedOut(void);
/* Resets the TWI peripheral after a timeout. It cannot free a slave that holds SDA low. */
void I2C_AVR_Recover(void);

#endif
