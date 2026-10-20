#ifndef APP_RETRY_H_
#define APP_RETRY_H_

#include <stdbool.h>

/* Capture the requested VOUT state before an ERROR turns the outputs off. */
void AppRetry_NoteShutdown(void);

/* Try immediately; used by `mode retry` and the timed retry task. */
bool AppRetry_Attempt(void);

/* Called periodically from the scheduler. Disabled in MANUAL or by configuration. */
void AppRetry_Update(void);

#endif /* APP_RETRY_H_ */
