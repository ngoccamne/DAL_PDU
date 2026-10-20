#include "app_setpoint.h"

#include <stdbool.h>

#include "app_event.h"
#include "app_mode.h"
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/TPS55287/bsp_tps55287.h"

/* Channel of the rail monitor that measures VOUT. */
#define CHANNEL_VOUT 0U

static bool s_active;
static bool s_mismatch;
static uint8_t s_count;
static uint16_t s_setpoint_mv;
static uint32_t s_settle_start_ms;

static uint32_t Distance(uint32_t a, uint32_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

static uint32_t Tolerance(uint16_t setpoint_mv)
{
    uint32_t tolerance = setpoint_mv / APP_SETPOINT_TOLERANCE_DIVISOR;
    return (tolerance > APP_SETPOINT_TOLERANCE_FLOOR_MV) ? tolerance : APP_SETPOINT_TOLERANCE_FLOOR_MV;
}

static void ClearMismatch(void)
{
    s_mismatch = false;
    s_count = 0U;
    AppMode_SetSetpointMismatch(false);
}

void AppSetpoint_Update(void)
{
    BSP_TPS55287_Status tps = BSP_TPS55287_GetStatus();
    SystemData_PowerStatus power = SystemData_GetPowerStatus();
    SystemData_PowerMonitorStatus monitor = SystemData_GetPowerMonitorStatus();
    bool vout_on = false;
    bool active = tps.setpoint_valid && power.trans_on && monitor.valid &&
                  BSP_TPS55287_IsEnabled(&vout_on) && vout_on;
    uint32_t now = SCH_SystemTick();
    uint32_t measured;

    if (!active)
    {
        s_active = false;
        ClearMismatch();
        return;
    }
    if (!s_active || tps.setpoint_mv != s_setpoint_mv)
    {
        /* A new setpoint, or an output that was just enabled, restarts the settling time. */
        s_active = true;
        s_setpoint_mv = tps.setpoint_mv;
        s_settle_start_ms = now;
        ClearMismatch();
        return;
    }
    if ((uint32_t)(now - s_settle_start_ms) < APP_SETPOINT_SETTLE_MS) return;

    measured = monitor.voltage_mv[CHANNEL_VOUT];
    if (Distance(measured, tps.setpoint_mv) <= Tolerance(tps.setpoint_mv))
    {
        ClearMismatch();
        return;
    }
    if (s_count < APP_SETPOINT_MISMATCH_SAMPLES) s_count++;
    if (s_count >= APP_SETPOINT_MISMATCH_SAMPLES && !s_mismatch)
    {
        s_mismatch = true;
        AppMode_SetSetpointMismatch(true);
        AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_SETPOINT_MISMATCH, (int16_t)measured);
    }
}
