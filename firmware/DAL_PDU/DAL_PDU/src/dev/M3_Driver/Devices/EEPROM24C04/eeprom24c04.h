#ifndef EEPROM24C04_H_
#define EEPROM24C04_H_

#include <stdbool.h>
#include <stdint.h>

#define EEPROM24C04_SIZE_BYTES 512U
#define EEPROM24C04_PAGE_SIZE  16U
#define EEPROM24C04_BASE_ADDRESS 0x50U

/* Board-provided I2C transfers. Device address is 0x50 or 0x51. */
bool EEPROM24C04_BusRead(uint8_t device_address, uint8_t word_address,
                         uint8_t *data, uint8_t length);
bool EEPROM24C04_BusWritePage(uint8_t device_address, uint8_t word_address,
                              const uint8_t *data, uint8_t length);
bool EEPROM24C04_BusReady(uint8_t device_address);

/* Address is a byte offset, 0..511. Zero-length operations succeed. */
bool EEPROM24C04_Read(uint16_t address, uint8_t *data, uint16_t length);
bool EEPROM24C04_Write(uint16_t address, const uint8_t *data, uint16_t length);

#endif /* EEPROM24C04_H_ */
