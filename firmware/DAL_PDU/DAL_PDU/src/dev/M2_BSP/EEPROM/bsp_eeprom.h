#ifndef BSP_EEPROM_H_
#define BSP_EEPROM_H_

#include <stdbool.h>
#include <stdint.h>

/* PA0 drives WP: high protects the EEPROM, low permits a write. */
void BSP_EEPROM_Init(void);
bool BSP_EEPROM_Probe(void);
bool BSP_EEPROM_Read(uint16_t address, uint8_t *data, uint16_t length);
bool BSP_EEPROM_Write(uint16_t address, const uint8_t *data, uint16_t length);
/* Blanks the whole chip to 0xFF, which AppEvent already reads back as an empty ring.
 * Used only by `persist_reset confirm`. False if the chip does not acknowledge. */
bool BSP_EEPROM_EraseAll(void);

#endif /* BSP_EEPROM_H_ */
