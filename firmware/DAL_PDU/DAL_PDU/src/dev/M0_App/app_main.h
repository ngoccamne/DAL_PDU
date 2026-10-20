#ifndef APP_MAIN_H_
#define APP_MAIN_H_

#include <stdbool.h>

/* Initialize the heartbeat LED and both serial ports; queue startup messages. */
bool APP_Initialize(void);

/* Firmware entry point; enables interrupts after initialization. */
void APP_Main(void);

#endif /* APP_MAIN_H_ */
