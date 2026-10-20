#ifndef APP_LIMITS_H_
#define APP_LIMITS_H_

#include <stdbool.h>
#include <stdint.h>

#include "../M2_BSP/NVM/bsp_nvm.h"

/* Defaults, used when the internal EEPROM record is blank or damaged. The overcurrent
 * defaults are 1 A (decided 2026-10-06). Confirm all values on the bench. */
#define APP_LIMITS_TEMP_DEFAULT_TENTHS_C 850
#define APP_LIMITS_VOUT_DEFAULT_MA 1000U
#define APP_LIMITS_TRANS5V_DEFAULT_MA 1000U

/* Consecutive one-second samples needed before a limit trips. */
#define APP_LIMITS_TEMP_TRIP_SAMPLES 2U
#define APP_LIMITS_CURRENT_TRIP_SAMPLES 3U

/* Accepted ranges for the configured limits. */
#define APP_LIMITS_TEMP_MIN_TENTHS_C (-400)
#define APP_LIMITS_TEMP_MAX_TENTHS_C 1250
#define APP_LIMITS_CURRENT_MIN_MA 100U
#define APP_LIMITS_CURRENT_MAX_MA 5000U

/* Writes the compiled defaults to *limits. Does not store them. */
void AppLimits_Defaults(BSP_NVM_Limits *limits);

/* Loads the limits from internal EEPROM, or the defaults. Call before the monitor runs. */
void AppLimits_Init(void);

/* Returns the active limits. */
void AppLimits_Get(BSP_NVM_Limits *limits);

/* Validates, stores, and persists the limits. Returns false and changes nothing when a
 * value is out of range. */
bool AppLimits_Set(const BSP_NVM_Limits *limits);

/* Called once per second in every mode. Each trip is logged once as a WARNING event in
 * the 24C04. In NORMAL mode it also turns both outputs off and sets the limit ERROR. */
void AppLimits_Update(void);

/* True while any limit is exceeded right now. */
bool AppLimits_Active(void);

/* Clears a limit ERROR once the regulator responds and every limit is in range, and
 * clears the stored flag with it. Returns true when the ERROR cleared. */
bool AppLimits_Retry(bool regulator_ok);

#endif /* APP_LIMITS_H_ */
