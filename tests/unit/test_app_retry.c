#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_retry.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_mode.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_config.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/TPS55287/bsp_tps55287.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Power/bsp_power.h"

static unsigned failures;
static uint32_t now_ms;
static BSP_NVM_Config config = {1U, 3U, 5U, 1U, 5U};
static bool error, manual, limit_active, monitor_valid, regulator_ok, trans_ok;
static bool trans_on, vout_on, setpoint_ok, enable_ok;
static uint16_t last_setpoint;
static unsigned probes, trans_enables, shutdowns;
static bool fault_pc4, status_ok;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

uint32_t SCH_SystemTick(void) { return now_ms; }
void AppConfig_Get(BSP_NVM_Config *out) { *out = config; }
AppMode AppMode_Get(void) { return manual ? APP_MODE_MANUAL : (error ? APP_MODE_ERROR : APP_MODE_NORMAL); }
bool AppMode_RegulatorError(void) { return error; }
bool AppMode_LimitError(void) { return false; }
void AppMode_SetRegulatorFault(void) { error = true; }
bool AppLimits_Active(void) { return limit_active; }
bool AppLimits_Retry(bool ok) { if (!ok || limit_active) return false; error = false; return true; }
bool AppPower_ProbeRegulator(void) { probes++; return regulator_ok; }
bool AppPower_SetTrans(bool enable) { if (!trans_ok) return false; trans_on = enable; trans_enables++; return true; }
void AppPower_TurnOffOutputs(void) { trans_on = false; vout_on = false; shutdowns++; }
SystemData_PowerStatus SystemData_GetPowerStatus(void)
{
    SystemData_PowerStatus status = {0};
    status.trans_on = trans_on;
    return status;
}
SystemData_PowerMonitorStatus SystemData_GetPowerMonitorStatus(void)
{
    SystemData_PowerMonitorStatus status = {0};
    status.valid = monitor_valid;
    return status;
}
BSP_TPS55287_Status BSP_TPS55287_GetStatus(void)
{
    BSP_TPS55287_Status status = {0};
    status.setpoint_valid = true;
    status.setpoint_mv = 7000U;
    status.commanded_enable = vout_on;
    return status;
}
bool BSP_TPS55287_IsEnabled(bool *enabled) { *enabled = vout_on; return true; }
bool BSP_TPS55287_SetVoltageMv(uint16_t mv) { last_setpoint = mv; return setpoint_ok; }
bool BSP_TPS55287_SetEnabled(bool enabled) { if (!enable_ok) return false; vout_on = enabled; return true; }
BSP_PowerFaults BSP_Power_ReadFaults(void)
{
    BSP_PowerFaults faults = {fault_pc4, false};
    return faults;
}
bool BSP_TPS55287_ReadState(TPS55287_State *state)
{
    if (!status_ok) return false;
    *state = (TPS55287_State){0};
    return true;
}

int main(void)
{
    monitor_valid = regulator_ok = trans_ok = setpoint_ok = enable_ok = status_ok = true;
    trans_on = vout_on = true;
    AppRetry_NoteShutdown();
    AppPower_TurnOffOutputs();
    error = true;
    AppRetry_Update();
    now_ms = 4999U;
    AppRetry_Update();
    CHECK(probes == 0U && !trans_on);
    now_ms = 5000U;
    AppRetry_Update();
    CHECK(!error && trans_on && vout_on && last_setpoint == 7000U);
    CHECK(probes == 1U && trans_enables == 1U);

    /* A failed attempt remains in ERROR and is retried after the configured interval. */
    error = true;
    regulator_ok = false;
    AppRetry_Update();
    now_ms = 10000U;
    AppRetry_Update();
    CHECK(error && probes == 2U && trans_enables == 1U);
    config.retry_time_s = 2U;
    regulator_ok = true;
    now_ms = 11999U;
    AppRetry_Update();
    CHECK(probes == 2U);
    now_ms = 12000U;
    AppRetry_Update();
    CHECK(!error && probes == 3U);

    error = true;
    config.retry_enable = 0U;
    now_ms = 30000U;
    AppRetry_Update();
    CHECK(error && probes == 3U);
    config.retry_enable = 1U;
    manual = true;
    AppRetry_Update();
    CHECK(error && probes == 3U);
    manual = false;
    limit_active = true;
    CHECK(!AppRetry_Attempt() && error && probes == 3U);
    limit_active = false;
    monitor_valid = false;
    CHECK(!AppRetry_Attempt() && error && probes == 3U);
    monitor_valid = true;
    CHECK(AppRetry_Attempt() && !error);

    error = true;
    fault_pc4 = true;
    CHECK(!AppRetry_Attempt() && error && shutdowns >= 2U);
    fault_pc4 = false;

    if (failures != 0U) return 1;
    puts("app retry tests passed");
    return 0;
}
