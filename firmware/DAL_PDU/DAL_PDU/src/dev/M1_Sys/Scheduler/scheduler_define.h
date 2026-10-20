#ifndef SCHEDULER_DEFINE_H_
#define SCHEDULER_DEFINE_H_

/* Each registered task has its own period in 1 ms ticks. The timer ISR
 * checks at most MAX_TASKS tasks; keep this bound small on the AVR. */
#define MAX_TASKS 9U

/* Optional binary semaphores. */
// #define USE_SCH_SEMAPHORE
#ifdef USE_SCH_SEMAPHORE
#define MAX_SEMAPHORES 2U
#endif

/* Independent countdown timers, decremented by the 1 ms ISR. */
typedef enum SCH_SoftTimerTypedef
{
    SCH_TIM_FIRST = 0,
    SCH_TIM_LED = SCH_TIM_FIRST,
    SCH_TIM_WDT,
    SCH_TIM_LAST
} SCH_SoftTimerTypedef;

#endif /* SCHEDULER_DEFINE_H_ */
