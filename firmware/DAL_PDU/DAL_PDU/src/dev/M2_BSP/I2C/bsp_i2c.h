#ifndef BSP_I2C_H_
#define BSP_I2C_H_

#include <stdbool.h>
#include <stdint.h>

bool BSP_I2C_Init(void);
bool BSP_I2C_Probe(uint8_t address);
bool BSP_I2C_ReadByte(uint8_t address, uint8_t *value);
bool BSP_I2C_ReadRegister(uint8_t address, uint8_t reg, uint8_t *value);
bool BSP_I2C_WriteRegister(uint8_t address, uint8_t reg, uint8_t value);
bool BSP_I2C_ReadBlock(uint8_t address, uint8_t reg, uint8_t *data, uint8_t length);
bool BSP_I2C_SendCommand(uint8_t address, uint8_t command);
/* True after a transaction timed out, until a transaction completes. A NACK does not set it. */
bool BSP_I2C_BusFault(void);
/* Address of the device that caused the current bus fault, or 0. */
uint8_t BSP_I2C_FaultAddress(void);

#endif
