#ifndef APP_BOOT_H_
#define APP_BOOT_H_

#include <stdbool.h>
#include <stdint.h>

#include "../M2_BSP/NVM/bsp_nvm.h"

/* Boot causes stored in the boot record. */
#define APP_BOOT_CAUSE_UNKNOWN 0U
#define APP_BOOT_CAUSE_POWER_ON 1U
#define APP_BOOT_CAUSE_BROWN_OUT 2U
#define APP_BOOT_CAUSE_EXTERNAL 3U
#define APP_BOOT_CAUSE_AVR_WATCHDOG 4U
#define APP_BOOT_CAUSE_CLIENT_WATCHDOG 5U
#define APP_BOOT_CAUSE_COMMAND 6U

typedef struct
{
    uint16_t boot_count;
    uint8_t cause;
    uint8_t source;   /* Watchdog source when cause is CLIENT_WATCHDOG, else 0. */
} AppBoot_Info;

/* Captures the reset flags, records the boot in internal EEPROM, and keeps the result for
 * bootinfo. Call first in APP_Initialize. */
void AppBoot_Init(void);
AppBoot_Info AppBoot_Get(void);
/* Boot history, newest first: index 0 is the last boot. Returns false past the stored boots. */
bool AppBoot_History(uint8_t index, BSP_NVM_BootRecord *record);

#endif /* APP_BOOT_H_ */
