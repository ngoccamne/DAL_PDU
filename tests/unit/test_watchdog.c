#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_watchdog.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Scheduler/scheduler.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Watchdog/bsp_watchdog.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"

volatile uint8_t DDRB, PORTB, PINB;
static unsigned int failures;
#define CONSOLE_TASK_HANDLE 0U

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expression); \
        failures++; \
    } \
} while (0)

static void console_task(void)
{
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_CONSOLE);
}

static void heartbeat_task(void)
{
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_HEARTBEAT);
}

static void modbus_task(void)
{
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_MODBUS);
}

static void monitor_task(void)
{
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_POWER_MONITOR);
}

static void temperature_task(void)
{
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_TEMPERATURE);
}

static void tick_n(unsigned int count)
{
    while (count-- > 0U)
    {
        SCH_RunSystemTickTimer();
        SCH_HandleScheduledTask();
    }
}

static void setup_monitor(void)
{
    static const SCH_TaskContext tasks[] =
    {
        {console_task, 10U, 0U, FALSE, TRUE},
        {heartbeat_task, 20U, 0U, FALSE, TRUE},
        {modbus_task, 10U, 0U, FALSE, TRUE},
        {monitor_task, 1000U, 999U, FALSE, TRUE},
        {temperature_task, 1000U, 999U, FALSE, TRUE},
        {AppWatchdog_Update, 10U, 0U, FALSE, TRUE}
    };

    DDRB = _BV(PB5) | _BV(2);
    PORTB = _BV(PB5) | _BV(PB6) | _BV(2);
    PINB = 0U;
    SystemData_Init();
    CHECK(SCH_InitializeTaskTable(tasks, 6U) == SCH_DONE);
    CHECK(AppWatchdog_Init());
    CHECK((DDRB & _BV(PB5)) == 0U);
    CHECK((DDRB & _BV(PB6)) != 0U);
    CHECK((PORTB & (_BV(PB5) | _BV(PB6))) == 0U);
    CHECK((PORTB & _BV(2)) != 0U);
    CHECK(!AppWatchdog_Start()); /* Both monitored apps are required. */
    CHECK(AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_CONSOLE, 200U));
    CHECK(!AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_CONSOLE, 200U));
    CHECK(AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_HEARTBEAT, 200U));
    CHECK(!AppWatchdog_Start()); /* Three clients are still unregistered. */
    CHECK(AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_MODBUS, APP_WATCHDOG_TIMEOUT_FAST_MS));
    CHECK(AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_POWER_MONITOR, APP_WATCHDOG_TIMEOUT_SLOW_MS));
    CHECK(AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_TEMPERATURE, APP_WATCHDOG_TIMEOUT_SLOW_MS));
    CHECK(AppWatchdog_Start());
    SCH_StartScheduler();
}

static void test_periodic_done_pulses_ignore_wake(void)
{
    setup_monitor();
    tick_n(990U);
    CHECK((PORTB & _BV(PB6)) == 0U);
    PINB = _BV(PB5); /* WAKE does not trigger the feed. */
    tick_n(10U);
    CHECK((PORTB & _BV(PB6)) != 0U); /* First pulse after one second. */
    tick_n(99U);
    CHECK((PORTB & _BV(PB6)) != 0U);
    tick_n(1U);
    CHECK((PORTB & _BV(PB6)) == 0U);
    PINB = 0U;
    tick_n(890U);
    CHECK((PORTB & _BV(PB6)) == 0U);
    tick_n(10U);
    CHECK((PORTB & _BV(PB6)) != 0U); /* Next pulse at two seconds. */
    tick_n(100U);
    CHECK((PORTB & _BV(PB6)) == 0U);
    CHECK(!AppWatchdog_IsFaulted());
}

static void test_missed_app_checkin_inhibits_done(void)
{
    setup_monitor();
    tick_n(1000U);
    CHECK((PORTB & _BV(PB6)) != 0U);
    CHECK(SCH_TASK_StopTask(CONSOLE_TASK_HANDLE) == SCH_DONE);
    tick_n(210U);
    CHECK(AppWatchdog_IsFaulted());
    CHECK((PORTB & _BV(PB6)) == 0U);

    PINB = _BV(PB5);
    tick_n(1U);
    PINB = 0U;
    tick_n(30U);
    CHECK((PORTB & _BV(PB6)) == 0U);
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_CONSOLE);
    CHECK(AppWatchdog_IsFaulted()); /* Fault remains latched. */
}

static void test_fault_stops_future_pulses(void)
{
    setup_monitor();
    tick_n(1000U);
    CHECK((PORTB & _BV(PB6)) != 0U);
    CHECK(SCH_TASK_StopTask(CONSOLE_TASK_HANDLE) == SCH_DONE);
    tick_n(210U);
    CHECK(AppWatchdog_IsFaulted());
    tick_n(1000U);
    CHECK((PORTB & _BV(PB6)) == 0U);
}

static void test_stalled_dispatch_cannot_recover_feed(void)
{
    unsigned int i;

    setup_monitor();
    tick_n(20U);
    for (i = 0U; i < 201U; i++)
    {
        SCH_RunSystemTickTimer();
    }
    SCH_HandleScheduledTask();
    CHECK(AppWatchdog_IsFaulted());
    CHECK((PORTB & _BV(PB6)) == 0U);
}

int main(void)
{
    test_periodic_done_pulses_ignore_wake();
    test_missed_app_checkin_inhibits_done();
    test_fault_stops_future_pulses();
    test_stalled_dispatch_cannot_recover_feed();
    if (failures != 0U)
    {
        fprintf(stderr, "%u watchdog checks failed\n", failures);
        return 1;
    }
    puts("watchdog tests passed");
    return 0;
}
