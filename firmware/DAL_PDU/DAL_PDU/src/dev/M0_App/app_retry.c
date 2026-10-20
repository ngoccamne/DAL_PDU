#include "app_retry.h"

#include <stdint.h>

#include "app_config.h"
#include "app_limits.h"
#include "app_mode.h"
#include "app_power.h"
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/TPS55287/bsp_tps55287.h"
#include "../M2_BSP/Power/bsp_power.h"

static bool s_restore_vout;
static uint16_t s_restore_mv;
static bool s_waiting;
static uint32_t s_wait_since_ms;

static bool FailRestore(void)
{
    AppMode_SetRegulatorFault();
    AppPower_TurnOffOutputs();
    return false;
}

void AppRetry_NoteShutdown(void)
{
    BSP_TPS55287_Status status = BSP_TPS55287_GetStatus();
    bool enabled = false;
    s_restore_vout = false;
    if (SystemData_GetPowerStatus().trans_on && status.setpoint_valid &&
        (status.commanded_enable || (BSP_TPS55287_IsEnabled(&enabled) && enabled)))
    {
        s_restore_vout = true;
        s_restore_mv = status.setpoint_mv;
    }
}

bool AppRetry_Attempt(void)
{
    SystemData_PowerMonitorStatus monitor = SystemData_GetPowerMonitorStatus();
    TPS55287_State state;
    if (!(AppMode_RegulatorError() || AppMode_LimitError())) return false;
    if (AppLimits_Active() || !monitor.valid || !AppPower_ProbeRegulator()) return false;
    if (!AppLimits_Retry(true)) return false;
    if (!AppPower_SetTrans(true)) return FailRestore();
    if (s_restore_vout &&
        (!BSP_TPS55287_SetVoltageMv(s_restore_mv) || !BSP_TPS55287_SetEnabled(true)))
        return FailRestore();
    if (BSP_Power_ReadFaults().pc4 || !BSP_TPS55287_ReadState(&state) ||
        (state.status & 0xE0U) != 0U) return FailRestore();
    s_waiting = false;
    return true;
}

void AppRetry_Update(void)
{
    BSP_NVM_Config config;
    uint32_t now = SCH_SystemTick();
    AppConfig_Get(&config);
    if (!config.retry_enable || AppMode_Get() == APP_MODE_MANUAL ||
        !(AppMode_RegulatorError() || AppMode_LimitError()))
    {
        s_waiting = false;
        return;
    }
    if (!s_waiting)
    {
        s_wait_since_ms = now;
        s_waiting = true;
        return;
    }
    if ((uint32_t)(now - s_wait_since_ms) < (uint32_t)config.retry_time_s * 1000UL) return;
    s_wait_since_ms = now;
    (void)AppRetry_Attempt();
}
