#ifndef BSP_WATCHDOG_H_
#define BSP_WATCHDOG_H_

#include <stdbool.h>

/* Configure PB5 as WAKE input and PB6 as DONE output, initially low. */
bool BSP_Watchdog_Init(void);

/* Called from main-loop context. */
void BSP_Watchdog_SetDone(bool high);
void BSP_Watchdog_Inhibit(void);

#endif /* BSP_WATCHDOG_H_ */
