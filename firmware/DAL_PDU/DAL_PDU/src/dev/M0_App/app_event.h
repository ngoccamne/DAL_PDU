#ifndef APP_EVENT_H_
#define APP_EVENT_H_

#include <stdbool.h>
#include <stdint.h>

/* Severity values. WARNING is for limit and bus faults. INFO is for settings changes. */
#define APP_EVENT_SEVERITY_INFO 1U
#define APP_EVENT_SEVERITY_WARNING 2U

/* Event codes. The value is in the unit given for each code. */
#define APP_EVENT_TEMP_LIMIT 1U      /* board temperature, tenths of degC */
#define APP_EVENT_VOUT_CURRENT 2U    /* VOUT current, mA */
#define APP_EVENT_TRANS_CURRENT 3U   /* +5_TRANS current, mA */
#define APP_EVENT_I2C_TIMEOUT 4U     /* I2C bus timeout, value 0 */
#define APP_EVENT_CONFIG_CHANGED 5U  /* config or limits changed, value 0 */
#define APP_EVENT_CLOCK_SET 7U      /* clock set from the console or Modbus, value 0 */
#define APP_EVENT_SETPOINT_MISMATCH 6U /* VOUT measured when the mismatch is flagged, mV */

typedef struct
{
    uint16_t seq;
    uint8_t severity;
    uint8_t code;
    uint8_t mode;
    int16_t value;
    uint32_t time_s; /* Unix seconds, or APP_CLOCK_UNSET when the clock was not set. */
    uint8_t clock_source; /* APP_CLOCK_SOURCE_*: where the time came from. */
} AppEvent;

/* Appends an event to the ring in the 24C04. Returns false if the EEPROM does not
 * accept the write. */
bool AppEvent_Log(uint8_t severity, uint8_t code, int16_t value);

/* Reads one ring slot, 0..APP_EVENT_SLOTS-1. Returns false for an empty or damaged slot. */
bool AppEvent_Read(uint8_t slot, AppEvent *event);

/* Finds the event with this sequence number. Sequence 0 finds the newest event. Returns false
 * when no stored event matches. */
bool AppEvent_Find(uint16_t seq, AppEvent *event);

#define APP_EVENT_SLOTS 32U

#endif /* APP_EVENT_H_ */
