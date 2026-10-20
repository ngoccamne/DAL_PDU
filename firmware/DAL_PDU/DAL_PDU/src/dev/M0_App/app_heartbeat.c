#include "app_heartbeat.h"
#include "app_mode.h"
#include "app_watchdog.h"

#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/LED/bsp_led.h"

typedef enum
{
    HEARTBEAT_ON,
    HEARTBEAT_OFF
} AppHeartbeat_State;

typedef struct
{
    uint16_t on_ms;
    uint16_t off_ms;
} AppHeartbeat_Step;

static const AppHeartbeat_Step s_error_steps[] =
{
    {50U, 50U}
};

static const AppHeartbeat_Step s_warning_steps[] =
{
    {100U, 100U},
    {100U, 1000U}
};

static const AppHeartbeat_Step s_manual_steps[] =
{
    {200U, 800U}
};

static AppHeartbeat_State s_state;
static SystemData_HeartbeatTiming s_active_timing;
static bool s_was_enabled;
static AppMode s_mode_seen = APP_MODE_NORMAL;
static bool s_pattern_active;
static bool s_pattern_on;
static uint8_t s_pattern_step;

static void AppHeartbeat_SetLed(bool on)
{
    BSP_LED_Set(on);
    SystemData_SetHeartbeatLedOn(on);
}

static void AppHeartbeat_RunPattern(const AppHeartbeat_Step *steps, uint8_t count)
{
    if (!s_pattern_active)
    {
        s_pattern_active = true;
        s_pattern_step = 0U;
        s_pattern_on = true;
        AppHeartbeat_SetLed(true);
        SCH_TIM_Start(SCH_TIM_LED, steps[0].on_ms);
    }
    else if (SCH_TIM_isCompleted(SCH_TIM_LED))
    {
        if (s_pattern_on)
        {
            s_pattern_on = false;
            AppHeartbeat_SetLed(false);
            SCH_TIM_Start(SCH_TIM_LED, steps[s_pattern_step].off_ms);
        }
        else
        {
            s_pattern_step = (uint8_t)((s_pattern_step + 1U) % count);
            s_pattern_on = true;
            AppHeartbeat_SetLed(true);
            SCH_TIM_Start(SCH_TIM_LED, steps[s_pattern_step].on_ms);
        }
    }
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_HEARTBEAT);
}

static void AppHeartbeat_NormalUpdate(void)
{
    SystemData_HeartbeatTiming timing = SystemData_GetHeartbeatTiming();
    SystemData_HeartbeatStatus status = SystemData_GetHeartbeatStatus();

    if (!status.enabled)
    {
        s_was_enabled = false;
        if (status.led_on) AppHeartbeat_SetLed(false);
        AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_HEARTBEAT);
        return;
    }
    if (!s_was_enabled)
    {
        s_was_enabled = true;
        s_state = HEARTBEAT_ON;
        s_active_timing = timing;
        AppHeartbeat_SetLed(true);
        SCH_TIM_Start(SCH_TIM_LED, s_active_timing.on_ms);
        AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_HEARTBEAT);
        return;
    }

    /* A new setting starts a full period in the current LED state. */
    if (timing.on_ms != s_active_timing.on_ms ||
        timing.off_ms != s_active_timing.off_ms)
    {
        s_active_timing = timing;
        SCH_TIM_Start(SCH_TIM_LED,
                      (s_state == HEARTBEAT_ON) ? timing.on_ms : timing.off_ms);
        AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_HEARTBEAT);
        return;
    }

    if (!SCH_TIM_isCompleted(SCH_TIM_LED))
    {
        AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_HEARTBEAT);
        return;
    }

    if (s_state == HEARTBEAT_ON)
    {
        s_state = HEARTBEAT_OFF;
        AppHeartbeat_SetLed(false);
        SCH_TIM_Start(SCH_TIM_LED, s_active_timing.off_ms);
    }
    else
    {
        s_state = HEARTBEAT_ON;
        AppHeartbeat_SetLed(true);
        SCH_TIM_Start(SCH_TIM_LED, s_active_timing.on_ms);
    }
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_HEARTBEAT);
}

bool AppHeartbeat_Init(void)
{
    if (!BSP_LED_Init())
    {
        return false;
    }

    s_active_timing = SystemData_GetHeartbeatTiming();
    s_state = HEARTBEAT_ON;
    s_was_enabled = true;
    AppHeartbeat_SetLed(true);
    SCH_TIM_Start(SCH_TIM_LED, s_active_timing.on_ms);

    if (!AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_HEARTBEAT, 200U))
    {
        AppHeartbeat_SetLed(false);
        return false;
    }
    return true;
}

void AppHeartbeat_Update(void)
{
    AppMode mode = AppMode_Get();

    if (mode != s_mode_seen)
    {
        s_mode_seen = mode;
        s_pattern_active = false;
        s_was_enabled = false;
    }

    switch (mode)
    {
        case APP_MODE_ERROR:
            AppHeartbeat_RunPattern(s_error_steps, 1U);
            break;
        case APP_MODE_WARNING:
            AppHeartbeat_RunPattern(s_warning_steps, 2U);
            break;
        case APP_MODE_MANUAL:
            AppHeartbeat_RunPattern(s_manual_steps, 1U);
            break;
        case APP_MODE_NORMAL:
        default:
            AppHeartbeat_NormalUpdate();
            break;
    }
}
