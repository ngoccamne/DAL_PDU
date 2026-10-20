#ifndef APP_CLOCK_H_
#define APP_CLOCK_H_

#include <stdbool.h>
#include <stdint.h>

/* Reported when the clock has not been set or restored. */
#define APP_CLOCK_UNSET 0xFFFFFFFFUL

/* Where the clock value came from. */
#define APP_CLOCK_SOURCE_UNSET 0U
#define APP_CLOCK_SOURCE_RESTORED 1U /* last checkpoint from EEPROM after a reset; approximate */
#define APP_CLOCK_SOURCE_SET 2U      /* set from the console or Modbus since the last reset */

/* Interval between EEPROM checkpoints while the clock is set. */
#define APP_CLOCK_CHECKPOINT_S 900UL

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} AppClock_Calendar;

/* Restores the clock from the latest EEPROM checkpoint, as RESTORED, or leaves it unset.
 * Call first at startup. */
void AppClock_Init(void);

/* Sets the clock as SET and stores a checkpoint at once. Returns false for the reserved value. */
bool AppClock_Set(uint32_t unix_s);

/* Stores a checkpoint every APP_CLOCK_CHECKPOINT_S while the clock is set. Called once a second
 * from the main loop. */
void AppClock_Update(void);

/* Returns Unix seconds, or APP_CLOCK_UNSET. */
uint32_t AppClock_Now(void);

/* APP_CLOCK_SOURCE_UNSET, _RESTORED, or _SET. */
uint8_t AppClock_Source(void);

/* Splits Unix seconds into a UTC calendar date and time. */
void AppClock_ToCalendar(uint32_t unix_s, AppClock_Calendar *calendar);

#endif /* APP_CLOCK_H_ */
