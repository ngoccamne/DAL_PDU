/*
 * scheduler.h
 *
 */
#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

#ifndef TRUE
#define TRUE    1U
#endif
#ifndef FALSE
#define FALSE   0U
#endif
#ifndef NULL
#define NULL    ((void *)0)
#endif

#include "scheduler_define.h"

/* Exported types ------------------------------------------------------------*/
typedef void (*SCH_TASK)(void);

typedef uint8_t SCH_TASK_HANDLE;
#define SCH_INVALID_TASK_HANDLE UINT8_MAX

typedef enum
{
    SCH_DONE,
    SCH_ERROR
} SCH_Status;

typedef struct
{
    uint16_t taskPeriodInMS; /* 1..65535 ms; counted from registration/resume. */
    SCH_TASK taskFunction;
} SCH_TaskPropertyTypedef;

/* Static task table entry. For an enabled task with ready = FALSE, the first
 * run occurs after period_ms - elapsed_ms ticks. Keep elapsed_ms < period_ms. */
typedef struct
{
    SCH_TASK function;
    volatile uint16_t period_ms;
    volatile uint16_t elapsed_ms;
    volatile uint8_t ready;
    volatile uint8_t enabled;
} SCH_TaskContext;

#ifdef USE_SCH_SEMAPHORE
#define SCH_SEMAPHORE_HANDLE            uint8_t
#define SCH_INVALID_SEMAPHORE_HANDLE    0xFF

/**
 * @brief Binary semaphore.
 */
typedef struct SCH_SemaphoreTypedef
{
    uint8_t value;  ///< 0 = taken, 1 = available.
} SCH_SemaphoreTypedef;
#endif

/* Exported functions --------------------------------------------------------*/
void       SCH_Initialize(void);
/* Initialize from a fixed application task table. The entries are copied so
 * SCH_TASK_SetPeriod can still change an individual task at runtime. */
SCH_Status SCH_InitializeTaskTable(const SCH_TaskContext *tasks,
                                   uint8_t task_count);
void       SCH_StartScheduler(void);
void       SCH_StopScheduler(void);
void       SCH_RunSystemTickTimer(void);   ///< Call from Timer0 CTC ISR (1 ms).
void       SCH_HandleScheduledTask(void); ///< Call from main loop.
uint32_t   SCH_SystemTick(void);           ///< Active milliseconds since initialization; wraps after ~49.7 days.

void       SCH_TIM_Start(const SCH_SoftTimerTypedef timer, const uint32_t timeInMs);
uint8_t    SCH_TIM_isCompleted(const SCH_SoftTimerTypedef timer);

/* Properties are copied into a free slot; no caller-owned lifetime is needed.
 * A due flag coalesces missed periods into one pending execution. */
SCH_Status SCH_TASK_CreateTask(SCH_TASK_HANDLE *handle,
                               const SCH_TaskPropertyTypedef *property);
SCH_Status SCH_TASK_StopTask(SCH_TASK_HANDLE handle);
SCH_Status SCH_TASK_ResumeTask(SCH_TASK_HANDLE handle);
SCH_Status SCH_TASK_SetPeriod(SCH_TASK_HANDLE handle, uint16_t period_ms);

#ifdef USE_SCH_SEMAPHORE
SCH_Status SCH_Semaphore_Create(SCH_SEMAPHORE_HANDLE *pHandle, uint8_t initialValue);
SCH_Status SCH_Semaphore_Take(SCH_SEMAPHORE_HANDLE semHandle);
SCH_Status SCH_Semaphore_Give(SCH_SEMAPHORE_HANDLE semHandle);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H_ */
