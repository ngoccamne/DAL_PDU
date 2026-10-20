#include "app_main.h"

#include <avr/interrupt.h>
#include <stdint.h>


#include "app_console.h"
#include "app_boot.h"
#include "app_clock.h"
#include "app_config.h"
#include "app_mode.h"
#include "app_heartbeat.h"
#include "app_limits.h"
#include "app_modbus.h"
#include "app_power.h"
#include "app_power_monitor.h"
#include "app_retry.h"
#include "app_temperature.h"
#include "app_watchdog.h"
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/Timer/bsp_timer.h"
#include "../M2_BSP/I2C/bsp_i2c.h"
#include "../M2_BSP/EEPROM/bsp_eeprom.h"
#include "../M2_BSP/UART/bsp_uart.h"
#include "../M2_BSP/Watchdog/bsp_watchdog.h"
#include "../M2_BSP/PAC1934/bsp_pac1934.h"

/* Slow tasks start 50 ms into the run, so their first check-ins come early. */
#define APP_SLOW_TASK_PHASE_MS 950U

/* Each slow client checks in from the task it monitors. */
static void APP_PowerMonitorTask(void)
{
    AppPowerMonitor_Update();
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_POWER_MONITOR);
}

static void APP_TemperatureTask(void)
{
    AppTemperature_Update();
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_TEMPERATURE);
}

/* The clock checkpoint is due every 15 minutes, so a 1 s check is plenty. */
static void APP_ClockTask(void)
{
    AppClock_Update();
}

/* Modbus is polled every 5 ms, so a request is answered within one period. */
static void APP_ModbusTask(void)
{
    AppModbus_Update();
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_MODBUS);
}

static bool APP_RegisterWatchdogClients(void)
{
    return AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_MODBUS, APP_WATCHDOG_TIMEOUT_FAST_MS) &&
           AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_POWER_MONITOR, APP_WATCHDOG_TIMEOUT_SLOW_MS) &&
           AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_TEMPERATURE, APP_WATCHDOG_TIMEOUT_SLOW_MS);
}

/* Initial elapsed time phases the power check at 3 ms, then every 6 ms. */
static const SCH_TaskContext s_app_tasks[] =
{
    {AppConsole_Run, 10U, 0U, FALSE, TRUE},
    {AppHeartbeat_Update, 20U, 0U, FALSE, TRUE},
    {AppPower_Update, 6U, 3U, FALSE, TRUE},
    {APP_PowerMonitorTask, 1000U, APP_SLOW_TASK_PHASE_MS, FALSE, TRUE},
    {AppRetry_Update, 100U, 0U, FALSE, TRUE},
    {AppWatchdog_Update, 10U, 0U, FALSE, TRUE},
    {APP_TemperatureTask, 1000U, APP_SLOW_TASK_PHASE_MS, FALSE, TRUE},
    {APP_ModbusTask, 5U, 0U, FALSE, TRUE},
    {APP_ClockTask, 1000U, APP_SLOW_TASK_PHASE_MS, FALSE, TRUE}
};

static bool APP_WriteString(uart_number_t number, const char *message)
{
    while (*message != '\0')
    {
        if (!BSP_UART_Write(number, (uint8_t)*message))
        {
            return false;
        }
        message++;
    }
    return true;
}

bool APP_Initialize(void)
{
    AppClock_Init();
    AppBoot_Init();
    BSP_EEPROM_Init();
    if (!BSP_UART_InitBoard())
    {
        return false;
    }

    if (!APP_WriteString(UART_NUMBER_CONSOLE, "Console test message\r\n"))
    {
        BSP_UART_Deinit();
        return false;
    }

    SystemData_Init();
    AppLimits_Init();
    AppConfig_Init();
    {
        BSP_NVM_Config config;
        AppConfig_Get(&config);
        AppMode_SetLinkTimeoutMs((uint32_t)config.comms_timeout_s * 1000UL);
    }
    if (!AppPower_Init())
    {
        BSP_UART_Deinit();
        return false;
    }
    if (!BSP_I2C_Init())
    {
        BSP_UART_Deinit();
        return false;
    }
    /* The TPS55287 (U5) sits behind the TRANS eFuse, which stays off at boot by design,
     * so it cannot answer here. Its presence is checked on demand when TRANS is enabled. */
    AppMode_SelfTest(true, BSP_PAC1934_Detect(), BSP_EEPROM_Probe());
    if (SCH_InitializeTaskTable(s_app_tasks,
                                (uint8_t)(sizeof(s_app_tasks) / sizeof(s_app_tasks[0]))) != SCH_DONE ||
        !AppWatchdog_Init() ||
        !AppConsole_Init() ||
        !AppHeartbeat_Init() ||
        !AppModbus_Init() ||
        !APP_RegisterWatchdogClients() ||
        !AppWatchdog_Start())
    {
        BSP_Watchdog_Inhibit();
        BSP_UART_Deinit();
        SCH_StopScheduler();
        return false;
    }
    AppTemperature_Init();
    if (!BSP_Timer_Init1ms())
    {
        BSP_Watchdog_Inhibit();
        BSP_UART_Deinit();
        SCH_StopScheduler();
        return false;
    }
    SCH_StartScheduler();
    return true;
}

void APP_Main(void)
{
    if (!APP_Initialize())
    {
        for (;;)
        {
        }
    }
    sei();
    while (1)
    {
        SCH_HandleScheduledTask();
    }
}
