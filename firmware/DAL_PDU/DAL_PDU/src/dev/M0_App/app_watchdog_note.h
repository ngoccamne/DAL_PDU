#ifndef APP_WATCHDOG_NOTE_H_
#define APP_WATCHDOG_NOTE_H_

#include <stdbool.h>
#include <stdint.h>

/* The watchdog fault source is kept in a .noinit RAM record, which an external or watchdog
 * reset does not clear. It is validated by a magic byte and a check byte, so random RAM
 * after power-up is rejected. Boot code takes it once, then writes it to the boot record. */
void AppWatchdogNote_Set(uint8_t source);
bool AppWatchdogNote_Take(uint8_t *source);

#endif /* APP_WATCHDOG_NOTE_H_ */
