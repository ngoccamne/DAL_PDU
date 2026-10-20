#ifndef APP_WATCHDOG_H_
#define APP_WATCHDOG_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_WATCHDOG_CLIENT_CONSOLE = 0,
    APP_WATCHDOG_CLIENT_HEARTBEAT,
    APP_WATCHDOG_CLIENT_MODBUS,
    APP_WATCHDOG_CLIENT_POWER_MONITOR,
    APP_WATCHDOG_CLIENT_TEMPERATURE,
    APP_WATCHDOG_CLIENT_COUNT
} AppWatchdog_Client;

/* Fault source codes, kept in the boot record. A client source is its index plus one. */
#define APP_WATCHDOG_SOURCE_NONE 0U
#define APP_WATCHDOG_SOURCE_CONSOLE 1U
#define APP_WATCHDOG_SOURCE_HEARTBEAT 2U
#define APP_WATCHDOG_SOURCE_MODBUS 3U
#define APP_WATCHDOG_SOURCE_POWER_MONITOR 4U
#define APP_WATCHDOG_SOURCE_TEMPERATURE 5U
#define APP_WATCHDOG_SOURCE_MONITOR 0x10U /* a client stopped being checked on */

/* Fast clients check in every pass of a 10 ms or 20 ms task, or of the main loop. Slow
 * clients run once per second, so their timeout leaves 500 ms of margin. */
#define APP_WATCHDOG_TIMEOUT_FAST_MS 200U
#define APP_WATCHDOG_PERIOD_SLOW_MS 1000U
#define APP_WATCHDOG_TIMEOUT_SLOW_MS 1500U
typedef char app_watchdog_slow_timeout_check[(APP_WATCHDOG_TIMEOUT_SLOW_MS > APP_WATCHDOG_PERIOD_SLOW_MS) ? 1 : -1];

bool AppWatchdog_Init(void);
bool AppWatchdog_RegisterClient(AppWatchdog_Client client, uint16_t timeout_ms);
bool AppWatchdog_Start(void);
void AppWatchdog_CheckIn(AppWatchdog_Client client);
void AppWatchdog_Update(void);
bool AppWatchdog_IsFaulted(void);

#endif /* APP_WATCHDOG_H_ */
