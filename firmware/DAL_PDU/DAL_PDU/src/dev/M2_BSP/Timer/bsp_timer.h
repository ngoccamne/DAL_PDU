#ifndef BSP_TIMER_H_
#define BSP_TIMER_H_

#include <stdbool.h>

/* Configure Timer0 CTC to interrupt every 1 ms at the board clock. */
bool BSP_Timer_Init1ms(void);
void BSP_Timer_Deinit(void);

#endif /* BSP_TIMER_H_ */
