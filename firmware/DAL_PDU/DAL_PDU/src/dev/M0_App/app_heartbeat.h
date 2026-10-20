#ifndef APP_HEARTBEAT_H_
#define APP_HEARTBEAT_H_

#include <stdbool.h>

/* Initialize PA3 and its watchdog client. Starts in ON state. */
bool AppHeartbeat_Init(void);
void AppHeartbeat_Update(void);

#endif /* APP_HEARTBEAT_H_ */
