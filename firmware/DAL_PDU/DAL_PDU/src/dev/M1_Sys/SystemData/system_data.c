#include "system_data.h"
#include <util/atomic.h>

#define HEARTBEAT_DEFAULT_PERIOD_MS 1000UL

static SystemData_HeartbeatTiming s_heartbeat_timing =
{
    HEARTBEAT_DEFAULT_PERIOD_MS,
    HEARTBEAT_DEFAULT_PERIOD_MS
};
static volatile SystemData_PowerStatus s_power_status;
static volatile SystemData_HeartbeatStatus s_heartbeat_status;
static volatile SystemData_TemperatureStatus s_temperature_status;
/* Written and read only by main-loop tasks; keep interrupts enabled for soft UART. */
static SystemData_PowerMonitorStatus s_power_monitor_status;

void SystemData_Init(void)
{
    uint8_t channel;
    s_heartbeat_timing.on_ms = HEARTBEAT_DEFAULT_PERIOD_MS;
    s_heartbeat_timing.off_ms = HEARTBEAT_DEFAULT_PERIOD_MS;
    s_heartbeat_status.enabled = true;
    s_heartbeat_status.led_on = false;
    s_power_status.trans_on = false;
    s_power_status.fault_pc4 = false;
    s_power_status.fault_pc0 = false;
    s_power_status.latch_pc4 = false;
    s_power_status.latch_pc0 = false;
    s_temperature_status.valid = false;
    s_temperature_status.tenths_c = 0;
    s_power_monitor_status.valid = false;
    s_power_monitor_status.address = 0U;
    for (channel = 0U; channel < SYSTEM_DATA_POWER_CHANNELS; channel++)
    {
        s_power_monitor_status.voltage_mv[channel] = 0U;
        s_power_monitor_status.current_ma[channel] = 0U;
        s_power_monitor_status.power_mw[channel] = 0U;
    }
}

SystemData_HeartbeatTiming SystemData_GetHeartbeatTiming(void)
{
    return s_heartbeat_timing;
}

bool SystemData_SetHeartbeatTiming(uint32_t on_ms, uint32_t off_ms)
{
    if (on_ms == 0UL || off_ms == 0UL)
    {
        return false;
    }

    s_heartbeat_timing.on_ms = on_ms;
    s_heartbeat_timing.off_ms = off_ms;
    return true;
}

SystemData_HeartbeatStatus SystemData_GetHeartbeatStatus(void)
{
    SystemData_HeartbeatStatus status;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        status.enabled = s_heartbeat_status.enabled;
        status.led_on = s_heartbeat_status.led_on;
    }
    return status;
}

void SystemData_SetHeartbeatEnabled(bool enabled)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_heartbeat_status.enabled = enabled;
    }
}

void SystemData_SetHeartbeatLedOn(bool led_on)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_heartbeat_status.led_on = led_on;
    }
}

void SystemData_SetPowerStatus(SystemData_PowerStatus status)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_power_status.trans_on = status.trans_on;
        s_power_status.fault_pc4 = status.fault_pc4;
        s_power_status.fault_pc0 = status.fault_pc0;
        s_power_status.latch_pc4 = status.latch_pc4;
        s_power_status.latch_pc0 = status.latch_pc0;
    }
}

SystemData_PowerStatus SystemData_GetPowerStatus(void)
{
    SystemData_PowerStatus status;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        status.trans_on = s_power_status.trans_on;
        status.fault_pc4 = s_power_status.fault_pc4;
        status.fault_pc0 = s_power_status.fault_pc0;
        status.latch_pc4 = s_power_status.latch_pc4;
        status.latch_pc0 = s_power_status.latch_pc0;
    }
    return status;
}

void SystemData_SetTemperatureStatus(SystemData_TemperatureStatus status)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_temperature_status.valid = status.valid;
        s_temperature_status.tenths_c = status.tenths_c;
    }
}

SystemData_TemperatureStatus SystemData_GetTemperatureStatus(void)
{
    SystemData_TemperatureStatus status;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        status.valid = s_temperature_status.valid;
        status.tenths_c = s_temperature_status.tenths_c;
    }
    return status;
}

void SystemData_SetPowerMonitorStatus(SystemData_PowerMonitorStatus status)
{
    uint8_t channel;
    s_power_monitor_status.valid = status.valid;
    s_power_monitor_status.address = status.address;
    for (channel = 0U; channel < SYSTEM_DATA_POWER_CHANNELS; channel++)
    {
        s_power_monitor_status.voltage_mv[channel] = status.voltage_mv[channel];
        s_power_monitor_status.current_ma[channel] = status.current_ma[channel];
        s_power_monitor_status.power_mw[channel] = status.power_mw[channel];
    }
}

SystemData_PowerMonitorStatus SystemData_GetPowerMonitorStatus(void)
{
    SystemData_PowerMonitorStatus status;
    uint8_t channel;
    status.valid = s_power_monitor_status.valid;
    status.address = s_power_monitor_status.address;
    for (channel = 0U; channel < SYSTEM_DATA_POWER_CHANNELS; channel++)
    {
        status.voltage_mv[channel] = s_power_monitor_status.voltage_mv[channel];
        status.current_ma[channel] = s_power_monitor_status.current_ma[channel];
        status.power_mw[channel] = s_power_monitor_status.power_mw[channel];
    }
    return status;
}
