#ifndef SYSTEM_DATA_H_
#define SYSTEM_DATA_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t on_ms;
    uint32_t off_ms;
} SystemData_HeartbeatTiming;

typedef struct
{
    bool enabled;
    bool led_on;
} SystemData_HeartbeatStatus;

typedef struct
{
    bool trans_on;
    bool fault_pc4;
    bool fault_pc0;
    bool latch_pc4;
    bool latch_pc0;
} SystemData_PowerStatus;

/* Board temperature in tenths of a degree C. Invalid until a reading succeeds. */
typedef struct
{
    bool valid;
    int16_t tenths_c;
} SystemData_TemperatureStatus;

#define SYSTEM_DATA_POWER_CHANNELS 4U
typedef struct
{
    bool valid;
    uint8_t address;
    uint16_t voltage_mv[SYSTEM_DATA_POWER_CHANNELS];
    uint32_t current_ma[SYSTEM_DATA_POWER_CHANNELS];
    uint32_t power_mw[SYSTEM_DATA_POWER_CHANNELS];
} SystemData_PowerMonitorStatus;

/* Called once at startup; both phases default to 1000 ms. */
void SystemData_Init(void);

/* Called from main-loop context by the command handler and heartbeat task. */
SystemData_HeartbeatTiming SystemData_GetHeartbeatTiming(void);
bool SystemData_SetHeartbeatTiming(uint32_t on_ms, uint32_t off_ms);
SystemData_HeartbeatStatus SystemData_GetHeartbeatStatus(void);
void SystemData_SetHeartbeatEnabled(bool enabled);
void SystemData_SetHeartbeatLedOn(bool led_on);
void SystemData_SetPowerStatus(SystemData_PowerStatus status);
SystemData_PowerStatus SystemData_GetPowerStatus(void);
void SystemData_SetTemperatureStatus(SystemData_TemperatureStatus status);
SystemData_TemperatureStatus SystemData_GetTemperatureStatus(void);

void SystemData_SetPowerMonitorStatus(SystemData_PowerMonitorStatus status);
SystemData_PowerMonitorStatus SystemData_GetPowerMonitorStatus(void);

#endif /* SYSTEM_DATA_H_ */
