#include <stdio.h>
#include <stdint.h>

#include "scheduler.h"

static unsigned int failures;
static unsigned int console_runs;
static unsigned int slow_runs;

static void console_task(void)
{
    console_runs++;
}

static void slow_task(void)
{
    slow_runs++;
}

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expression); \
        failures++; \
    } \
} while (0)

static void tick_n(uint32_t count)
{
    while (count-- > 0U)
    {
        SCH_RunSystemTickTimer();
        SCH_HandleScheduledTask();
    }
}

static void test_stopped_scheduler_does_not_advance(void)
{
    SCH_Initialize();
    SCH_TIM_Start(SCH_TIM_LED, 2U);
    tick_n(10U);

    CHECK(SCH_SystemTick() == 0U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == FALSE);

    SCH_StartScheduler();
    tick_n(1U);
    CHECK(SCH_SystemTick() == 1U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == FALSE);
    tick_n(1U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == TRUE);
}

static void test_timers_are_independent_and_expire_on_exact_tick(void)
{
    SCH_Initialize();
    SCH_StartScheduler();
    SCH_TIM_Start(SCH_TIM_LED, 3U);
    SCH_TIM_Start(SCH_TIM_WDT, 7U);

    tick_n(2U);
    CHECK(SCH_SystemTick() == 2U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == FALSE);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_WDT) == FALSE);

    tick_n(1U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == TRUE);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_WDT) == FALSE);

    tick_n(4U);
    CHECK(SCH_SystemTick() == 7U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_WDT) == TRUE);
    tick_n(10U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == TRUE);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_WDT) == TRUE);
}

static void test_timer_restart_zero_and_invalid_handle(void)
{
    SCH_Initialize();
    SCH_StartScheduler();
    SCH_TIM_Start(SCH_TIM_LED, 1U);
    tick_n(1U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == TRUE);

    SCH_TIM_Start(SCH_TIM_LED, 4U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == FALSE);
    tick_n(3U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == FALSE);
    SCH_TIM_Start(SCH_TIM_LED, 0U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LED) == TRUE);

    SCH_TIM_Start(SCH_TIM_LAST, 10U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_LAST) == TRUE);
}

static void test_stop_and_reinitialize(void)
{
    SCH_Initialize();
    SCH_StartScheduler();
    SCH_TIM_Start(SCH_TIM_WDT, 20U);
    tick_n(6U);
    SCH_StopScheduler();
    tick_n(10U);

    CHECK(SCH_SystemTick() == 6U);
    CHECK(SCH_TIM_isCompleted(SCH_TIM_WDT) == TRUE);

    SCH_StartScheduler();
    tick_n(1U);
    CHECK(SCH_SystemTick() == 7U);

    SCH_Initialize();
    CHECK(SCH_SystemTick() == 0U);
    tick_n(1U);
    CHECK(SCH_SystemTick() == 0U);
}

static void test_console_runs_every_ten_ms(void)
{
    SCH_TASK_HANDLE console;
    SCH_TASK_HANDLE slow;
    const SCH_TaskPropertyTypedef console_property = {10U, console_task};
    const SCH_TaskPropertyTypedef slow_property = {100U, slow_task};

    SCH_Initialize();
    console_runs = 0U;
    slow_runs = 0U;
    CHECK(SCH_TASK_CreateTask(&console, &console_property) == SCH_DONE);
    CHECK(SCH_TASK_CreateTask(&slow, &slow_property) == SCH_DONE);
    CHECK(console == 0U && slow == 1U);
    SCH_StartScheduler();
    tick_n(9U);
    CHECK(console_runs == 0U);
    tick_n(1U);
    CHECK(console_runs == 1U);
    tick_n(9U);
    CHECK(console_runs == 1U);
    tick_n(1U);
    CHECK(console_runs == 2U);
    tick_n(80U);
    CHECK(console_runs == 10U);
    CHECK(slow_runs == 1U);
}

static void test_static_task_table(void)
{
    static const SCH_TaskContext tasks[] =
    {
        {console_task, 10U, 0U, FALSE, TRUE},
        {slow_task, 20U, 15U, FALSE, TRUE}
    };
    static const SCH_TaskContext invalid_period[] =
    {
        {console_task, 0U, 0U, FALSE, TRUE}
    };
    static const SCH_TaskContext invalid_elapsed[] =
    {
        {console_task, 10U, 10U, FALSE, TRUE}
    };

    CHECK(SCH_InitializeTaskTable(NULL, 2U) == SCH_ERROR);
    CHECK(SCH_InitializeTaskTable(tasks, 0U) == SCH_ERROR);
    CHECK(SCH_InitializeTaskTable(invalid_period, 1U) == SCH_ERROR);
    CHECK(SCH_InitializeTaskTable(invalid_elapsed, 1U) == SCH_ERROR);
    CHECK(SCH_InitializeTaskTable(tasks, 2U) == SCH_DONE);
    console_runs = 0U;
    slow_runs = 0U;
    SCH_StartScheduler();
    tick_n(4U);
    CHECK(console_runs == 0U && slow_runs == 0U);
    tick_n(1U);
    CHECK(slow_runs == 1U); /* 20 ms period, 15 ms initial elapsed. */
    tick_n(15U);
    CHECK(console_runs == 2U && slow_runs == 1U);
    CHECK(SCH_TASK_SetPeriod(1U, 5U) == SCH_DONE);
    tick_n(5U);
    CHECK(slow_runs == 2U);
    CHECK(tasks[1].period_ms == 20U && tasks[1].elapsed_ms == 15U);
}

static void test_task_stop_resume_and_period_change(void)
{
    SCH_TASK_HANDLE task;
    const SCH_TaskPropertyTypedef property = {7U, console_task};

    SCH_Initialize();
    console_runs = 0U;
    CHECK(SCH_TASK_CreateTask(&task, &property) == SCH_DONE);
    SCH_StartScheduler();
    tick_n(6U);
    CHECK(console_runs == 0U);
    CHECK(SCH_TASK_StopTask(task) == SCH_DONE);
    tick_n(20U);
    CHECK(console_runs == 0U);
    CHECK(SCH_TASK_ResumeTask(task) == SCH_DONE);
    tick_n(7U);
    CHECK(console_runs == 1U);
    CHECK(SCH_TASK_SetPeriod(task, 3U) == SCH_DONE);
    tick_n(2U);
    CHECK(console_runs == 1U);
    tick_n(1U);
    CHECK(console_runs == 2U);
    CHECK(SCH_TASK_SetPeriod(task, 0U) == SCH_ERROR);
    CHECK(SCH_TASK_StopTask(SCH_INVALID_TASK_HANDLE) == SCH_ERROR);
    CHECK(SCH_TASK_ResumeTask(SCH_INVALID_TASK_HANDLE) == SCH_ERROR);
    CHECK(SCH_TASK_SetPeriod(SCH_INVALID_TASK_HANDLE, 10U) == SCH_ERROR);
}

static void test_registration_limits_and_pending_flag(void)
{
    SCH_TASK_HANDLE handle = SCH_INVALID_TASK_HANDLE;
    const SCH_TaskPropertyTypedef property = {1U, console_task};
    const SCH_TaskPropertyTypedef zero_period = {0U, console_task};
    const SCH_TaskPropertyTypedef no_function = {1U, NULL};
    uint8_t i;

    SCH_Initialize();
    console_runs = 0U;
    CHECK(SCH_TASK_CreateTask(NULL, &property) == SCH_ERROR);
    CHECK(SCH_TASK_CreateTask(&handle, NULL) == SCH_ERROR);
    CHECK(SCH_TASK_CreateTask(&handle, &zero_period) == SCH_ERROR);
    CHECK(SCH_TASK_CreateTask(&handle, &no_function) == SCH_ERROR);
    CHECK(handle == SCH_INVALID_TASK_HANDLE);
    for (i = 0U; i < MAX_TASKS; i++)
    {
        CHECK(SCH_TASK_CreateTask(&handle, &property) == SCH_DONE);
        CHECK(handle == i);
    }
    CHECK(SCH_TASK_CreateTask(&handle, &property) == SCH_ERROR);
    SCH_StartScheduler();
    SCH_RunSystemTickTimer();
    SCH_RunSystemTickTimer();
    SCH_RunSystemTickTimer();
    SCH_HandleScheduledTask();
    CHECK(console_runs == MAX_TASKS); /* Missed periods coalesce. */
    SCH_StopScheduler();
    CHECK(SCH_TASK_StopTask(0U) == SCH_ERROR);
    CHECK(SCH_TASK_CreateTask(&handle, &property) == SCH_DONE);
    CHECK(handle == 0U);
}

#ifdef USE_SCH_SEMAPHORE
static void test_semaphore_lifecycle_and_bounds(void)
{
    SCH_SEMAPHORE_HANDLE first;
    SCH_SEMAPHORE_HANDLE second;
    SCH_SEMAPHORE_HANDLE extra;

    SCH_Initialize();
    CHECK(SCH_Semaphore_Create(NULL, 1U) == SCH_ERROR);
    CHECK(SCH_Semaphore_Create(&first, 1U) == SCH_DONE);
    CHECK(SCH_Semaphore_Create(&second, 0U) == SCH_DONE);
    CHECK(SCH_Semaphore_Create(&extra, 1U) == SCH_ERROR);
    CHECK(SCH_Semaphore_Take(first) == SCH_DONE);
    CHECK(SCH_Semaphore_Take(first) == SCH_ERROR);
    CHECK(SCH_Semaphore_Give(first) == SCH_DONE);
    CHECK(SCH_Semaphore_Give(first) == SCH_ERROR);
    CHECK(SCH_Semaphore_Take(second) == SCH_ERROR);
    CHECK(SCH_Semaphore_Give(second) == SCH_DONE);
    CHECK(SCH_Semaphore_Take(SCH_INVALID_SEMAPHORE_HANDLE) == SCH_ERROR);

    SCH_StopScheduler();
    CHECK(SCH_Semaphore_Take(first) == SCH_ERROR);
    CHECK(SCH_Semaphore_Create(&first, 1U) == SCH_DONE);
    CHECK(first == 0U);
}
#endif

int main(void)
{
    test_stopped_scheduler_does_not_advance();
    test_timers_are_independent_and_expire_on_exact_tick();
    test_timer_restart_zero_and_invalid_handle();
    test_stop_and_reinitialize();
    test_console_runs_every_ten_ms();
    test_static_task_table();
    test_task_stop_resume_and_period_change();
    test_registration_limits_and_pending_flag();
#ifdef USE_SCH_SEMAPHORE
    test_semaphore_lifecycle_and_bounds();
#endif

    if (failures != 0U)
    {
        fprintf(stderr, "%u scheduler checks failed\n", failures);
        return 1;
    }

    puts("scheduler tests passed");
    return 0;
}
