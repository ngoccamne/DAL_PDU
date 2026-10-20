#include "scheduler.h"

#include <stddef.h>
#include <string.h>
#include <util/atomic.h>

static SCH_TaskContext s_Tasks[MAX_TASKS];
static volatile uint8_t s_TaskCount;
static volatile uint8_t s_SchedulerRunning;
static volatile uint32_t s_SystemTick;
static volatile uint32_t s_SoftTimers[SCH_TIM_LAST];

#ifdef USE_SCH_SEMAPHORE
static SCH_SemaphoreTypedef s_SemaphoreContext[MAX_SEMAPHORES];
static uint8_t s_NumOfSemaphores;
#endif

void SCH_Initialize(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_SchedulerRunning = FALSE;
        s_SystemTick = 0U;
        s_TaskCount = 0U;
        memset(s_Tasks, 0, sizeof(s_Tasks));
        memset((void *)s_SoftTimers, 0, sizeof(s_SoftTimers));
#ifdef USE_SCH_SEMAPHORE
        s_NumOfSemaphores = 0U;
        memset(s_SemaphoreContext, 0, sizeof(s_SemaphoreContext));
#endif
    }
}

SCH_Status SCH_InitializeTaskTable(const SCH_TaskContext *tasks,
                                   uint8_t task_count)
{
    uint8_t i;

    if (tasks == NULL || task_count == 0U || task_count > MAX_TASKS)
    {
        return SCH_ERROR;
    }
    for (i = 0U; i < task_count; i++)
    {
        if (tasks[i].function == NULL || tasks[i].period_ms == 0U ||
            tasks[i].elapsed_ms >= tasks[i].period_ms ||
            tasks[i].ready > TRUE || tasks[i].enabled > TRUE)
        {
            return SCH_ERROR;
        }
    }

    SCH_Initialize();
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        for (i = 0U; i < task_count; i++)
        {
            s_Tasks[i].function = tasks[i].function;
            s_Tasks[i].period_ms = tasks[i].period_ms;
            s_Tasks[i].elapsed_ms = tasks[i].elapsed_ms;
            s_Tasks[i].ready = tasks[i].ready;
            s_Tasks[i].enabled = tasks[i].enabled;
        }
        s_TaskCount = task_count;
    }
    return SCH_DONE;
}

SCH_Status SCH_TASK_CreateTask(SCH_TASK_HANDLE *handle,
                               const SCH_TaskPropertyTypedef *property)
{
    SCH_Status status = SCH_ERROR;

    if (handle == NULL || property == NULL || property->taskFunction == NULL ||
        property->taskPeriodInMS == 0U)
    {
        return SCH_ERROR;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (s_TaskCount < MAX_TASKS)
        {
            SCH_TaskContext *task = &s_Tasks[s_TaskCount];
            task->function = property->taskFunction;
            task->period_ms = property->taskPeriodInMS;
            task->elapsed_ms = 0U;
            task->ready = FALSE;
            task->enabled = TRUE;
            *handle = s_TaskCount;
            s_TaskCount++;
            status = SCH_DONE;
        }
    }
    return status;
}

SCH_Status SCH_TASK_StopTask(SCH_TASK_HANDLE handle)
{
    SCH_Status status = SCH_ERROR;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (handle < s_TaskCount)
        {
            s_Tasks[handle].enabled = FALSE;
            s_Tasks[handle].ready = FALSE;
            s_Tasks[handle].elapsed_ms = 0U;
            status = SCH_DONE;
        }
    }
    return status;
}

SCH_Status SCH_TASK_ResumeTask(SCH_TASK_HANDLE handle)
{
    SCH_Status status = SCH_ERROR;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (handle < s_TaskCount)
        {
            s_Tasks[handle].elapsed_ms = 0U;
            s_Tasks[handle].ready = FALSE;
            s_Tasks[handle].enabled = TRUE;
            status = SCH_DONE;
        }
    }
    return status;
}

SCH_Status SCH_TASK_SetPeriod(SCH_TASK_HANDLE handle, uint16_t period_ms)
{
    SCH_Status status = SCH_ERROR;
    if (period_ms == 0U) return SCH_ERROR;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (handle < s_TaskCount)
        {
            s_Tasks[handle].period_ms = period_ms;
            s_Tasks[handle].elapsed_ms = 0U;
            s_Tasks[handle].ready = FALSE;
            status = SCH_DONE;
        }
    }
    return status;
}

void SCH_TIM_Start(const SCH_SoftTimerTypedef timer, const uint32_t timeInMs)
{
    if (timer < SCH_TIM_LAST)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            s_SoftTimers[timer] = timeInMs;
        }
    }
}

uint8_t SCH_TIM_isCompleted(const SCH_SoftTimerTypedef timer)
{
    uint32_t remaining;
    if (timer >= SCH_TIM_LAST) return TRUE;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        remaining = s_SoftTimers[timer];
    }
    return (remaining == 0U) ? TRUE : FALSE;
}

/* Called only from the Timer0 compare ISR, once per millisecond. */
void SCH_RunSystemTickTimer(void)
{
    uint8_t i;
    if (!s_SchedulerRunning) return;

    s_SystemTick++;
    for (i = 0U; i < s_TaskCount; i++)
    {
        SCH_TaskContext *task = &s_Tasks[i];
        if (task->enabled)
        {
            task->elapsed_ms++;
            if (task->elapsed_ms >= task->period_ms)
            {
                task->elapsed_ms = 0U;
                task->ready = TRUE;
            }
        }
    }
    for (i = 0U; i < SCH_TIM_LAST; i++)
    {
        if (s_SoftTimers[i] > 0U) s_SoftTimers[i]--;
    }
}

void SCH_StartScheduler(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_SchedulerRunning = TRUE;
    }
}

void SCH_StopScheduler(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_SchedulerRunning = FALSE;
        s_TaskCount = 0U;
        memset(s_Tasks, 0, sizeof(s_Tasks));
        memset((void *)s_SoftTimers, 0, sizeof(s_SoftTimers));
#ifdef USE_SCH_SEMAPHORE
        s_NumOfSemaphores = 0U;
        memset(s_SemaphoreContext, 0, sizeof(s_SemaphoreContext));
#endif
    }
}

/* Task callbacks run in main-loop context. Each pending flag is consumed
 * atomically so an ISR tick cannot be lost while it is being cleared. */
void SCH_HandleScheduledTask(void)
{
    uint8_t i;
    if (!s_SchedulerRunning) return;

    for (i = 0U; i < s_TaskCount && s_SchedulerRunning; i++)
    {
        uint8_t ready;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            ready = (s_Tasks[i].ready && s_Tasks[i].enabled);
            s_Tasks[i].ready = FALSE;
        }
        if (ready && s_Tasks[i].function != NULL)
        {
            s_Tasks[i].function();
        }
    }
}

uint32_t SCH_SystemTick(void)
{
    uint32_t tick;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        tick = s_SystemTick;
    }
    return tick;
}

#ifdef USE_SCH_SEMAPHORE
SCH_Status SCH_Semaphore_Create(SCH_SEMAPHORE_HANDLE *pHandle, uint8_t initialValue)
{
    if (pHandle == NULL || s_NumOfSemaphores >= MAX_SEMAPHORES) return SCH_ERROR;
    s_SemaphoreContext[s_NumOfSemaphores].value = initialValue;
    *pHandle = s_NumOfSemaphores++;
    return SCH_DONE;
}

SCH_Status SCH_Semaphore_Take(SCH_SEMAPHORE_HANDLE semHandle)
{
    if (semHandle >= s_NumOfSemaphores || s_SemaphoreContext[semHandle].value == 0U)
        return SCH_ERROR;
    s_SemaphoreContext[semHandle].value--;
    return SCH_DONE;
}

SCH_Status SCH_Semaphore_Give(SCH_SEMAPHORE_HANDLE semHandle)
{
    if (semHandle >= s_NumOfSemaphores || s_SemaphoreContext[semHandle].value >= 1U)
        return SCH_ERROR;
    s_SemaphoreContext[semHandle].value++;
    return SCH_DONE;
}
#endif
