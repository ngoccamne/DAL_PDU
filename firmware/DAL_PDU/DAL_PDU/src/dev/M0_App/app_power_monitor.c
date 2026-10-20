#include "app_power_monitor.h"

#include "app_event.h"
#include "app_limits.h"
#include "app_mode.h"
#include "app_power.h"
#include "app_setpoint.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/I2C/bsp_i2c.h"
#include "../M2_BSP/PAC1934/bsp_pac1934.h"
#include "../M2_BSP/Power/bsp_power.h"
#include "../M2_BSP/TPS55287/bsp_tps55287.h"

/* TPS55287 status bits: SCP 0x80, OCP 0x40, OVP 0x20. */
#define REGULATOR_FAULT_MASK (0x80U | 0x40U | 0x20U)

static void AppPowerMonitor_CheckRegulator(void)
{
    TPS55287_State state;
    /* The TPS55287 is behind the TRANS eFuse and is off at idle by design; it is only
     * meaningful to poll while TRANS is actually on. */
    if (!AppMode_RegulatorMonitored() || !BSP_Power_IsTransOn() ||
        BSP_Power_ReadFaults().pc4) return;
    if (!BSP_TPS55287_ReadState(&state) || (state.status & REGULATOR_FAULT_MASK) != 0U)
    {
        AppMode_SetRegulatorFault();
    }
}

static void AppPowerMonitor_CheckLimits(void)
{
    AppLimits_Update();
}

static void AppPowerMonitor_CheckBus(void)
{
    static bool logged;
    bool fault = BSP_I2C_BusFault();
    AppMode_SetBusFault(fault);
    /* A regulator timeout is an ERROR while the regulator is monitored (NORMAL, WARNING). In MANUAL
     * it is a WARNING cause only. Other devices stay at WARNING. */
    if (fault && BSP_I2C_FaultAddress() == BSP_TPS55287_Address() && AppMode_RegulatorMonitored() &&
        BSP_Power_IsTransOn() && !BSP_Power_ReadFaults().pc4)
    {
        AppMode_SetRegulatorFault();
    }
    if (fault && !logged) AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_I2C_TIMEOUT, 0);
    logged = fault;
}

void AppPowerMonitor_Update(void)
{
    AppPowerMonitor_CheckBus();
    PAC1934_Sample sample;
    SystemData_PowerMonitorStatus status = SystemData_GetPowerMonitorStatus();
    uint8_t channel;
    bool readable = BSP_PAC1934_ReadSnapshot(&sample);

    AppMode_SetPacUnreadable(!readable);
    AppPowerMonitor_CheckRegulator();
    if (!readable)
    {
        status.valid = false;
        status.address = BSP_PAC1934_Address();
        SystemData_SetPowerMonitorStatus(status);
        AppPowerMonitor_CheckLimits();
        AppSetpoint_Update();
        return;
    }
    status.valid = true;
    status.address = BSP_PAC1934_Address();
    for (channel = 0U; channel < SYSTEM_DATA_POWER_CHANNELS; channel++)
    {
        status.voltage_mv[channel] = sample.voltage_mv[channel];
        status.current_ma[channel] = sample.current_ma[channel];
        status.power_mw[channel] = sample.power_mw[channel];
    }
    SystemData_SetPowerMonitorStatus(status);
    AppPowerMonitor_CheckLimits();
    AppSetpoint_Update();
}
