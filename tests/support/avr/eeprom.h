#ifndef TEST_AVR_EEPROM_H_
#define TEST_AVR_EEPROM_H_

#include <stdint.h>
#include <stddef.h>

extern uint8_t test_eeprom[4096];

static inline uint8_t eeprom_read_byte(const uint8_t *address)
{
    return test_eeprom[(uintptr_t)address];
}

static inline void eeprom_update_byte(uint8_t *address, uint8_t value)
{
    test_eeprom[(uintptr_t)address] = value;
}

#endif /* TEST_AVR_EEPROM_H_ */
