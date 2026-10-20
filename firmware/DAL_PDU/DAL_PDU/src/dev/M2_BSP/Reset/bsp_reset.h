#ifndef BSP_RESET_H_
#define BSP_RESET_H_

#include <stdbool.h>
#include <stdint.h>

/* Bit values match the ATmega128A MCUSR register. */
#define BSP_RESET_POWER_ON 0x01U
#define BSP_RESET_EXTERNAL 0x02U
#define BSP_RESET_BROWN_OUT 0x04U
#define BSP_RESET_WATCHDOG 0x08U

/* Returns the reset flags of the last reset, clears MCUSR, and disables the watchdog. Call
 * first in APP_Initialize, before anything else can reset the flags. Returns 0 on a host
 * build. */
uint8_t BSP_Reset_CaptureFlags(void);

/* True once if the last reset was requested by the console. Clears the request. */
bool BSP_Reset_TakeSoftwareRequest(void);

/* Restarts the MCU through the watchdog. Call only with the outputs off. Returns on a host
 * build. */
void BSP_Reset_Software(void);

/* Request the AVR bootloader via its reserved EEPROM bytes, then watchdog reset. */
void BSP_Reset_Bootloader(void);

#endif /* BSP_RESET_H_ */
