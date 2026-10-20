#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

#include "../M2_BSP/NVM/bsp_nvm.h"

/* Defaults, used when the internal EEPROM record is blank or damaged. */
#define APP_CONFIG_SLAVE_ADDRESS_DEFAULT 1U
#define APP_CONFIG_BAUD_CODE_DEFAULT 6U        /* 115200 baud */
#define APP_CONFIG_COMMS_TIMEOUT_DEFAULT_S 5U
#define APP_CONFIG_RETRY_ENABLE_DEFAULT 1U
#define APP_CONFIG_RETRY_TIME_DEFAULT_S 5U

/* Accepted ranges. */
#define APP_CONFIG_SLAVE_ADDRESS_MIN 1U
#define APP_CONFIG_SLAVE_ADDRESS_MAX 247U
#define APP_CONFIG_COMMS_TIMEOUT_MIN_S 1U
#define APP_CONFIG_COMMS_TIMEOUT_MAX_S 60U
#define APP_CONFIG_RETRY_TIME_MIN_S 1U
#define APP_CONFIG_RETRY_TIME_MAX_S 60U

/* Baud codes: 0 1200, 1 2400, 2 4800, 3 9600, 4 19200, 5 38400, 6 115200. Each rate is within 0.2 %
 * of the UART1 divider at 12 MHz. */
#define APP_CONFIG_BAUD_COUNT 7U

/* Loads the configuration from internal EEPROM, or the defaults. Call before Modbus starts. */
void AppConfig_Init(void);

/* Returns the active configuration. */
void AppConfig_Get(BSP_NVM_Config *config);

/* Validates and stores the configuration. Returns false and changes nothing when a value is
 * out of range. */
bool AppConfig_Set(const BSP_NVM_Config *config);

/* Stores the compiled defaults. */
void AppConfig_Reset(void);

/* True while the active values are the compiled defaults, because no valid record exists. */
bool AppConfig_DefaultsInUse(void);

/* Baud rate for a baud code, or 0 for an unknown code. */
uint32_t AppConfig_BaudRate(uint8_t baud_code);

/* Finds the baud code for a rate. Returns false for a rate that is not supported. */
bool AppConfig_BaudCodeFor(uint32_t baud, uint8_t *baud_code);

#endif /* APP_CONFIG_H_ */
