#include "app_mode.h"

#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"

static bool s_regulator_error;
static bool s_limit_error;
static bool s_pac_unreadable;
static bool s_bus_fault;
static bool s_eeprom_missing;
static bool s_setpoint_mismatch;
static bool s_manual;
static bool s_link_seen;
static uint32_t s_last_request_ms;
static uint32_t s_link_timeout_ms = APP_MODE_COMMS_TIMEOUT_MS;

void AppMode_SelfTest(bool tps55287_ok, bool pac1934_ok, bool eeprom_ok)
{
    s_regulator_error = !tps55287_ok;
    s_pac_unreadable = !pac1934_ok;
    s_eeprom_missing = !eeprom_ok;
}

AppMode AppMode_Get(void)
{
    SystemData_PowerStatus power;
    /* MANUAL outranks ERROR (decided 2026-10-15): once requested, the operator has taken
     * manual control and every command is accepted, including from ERROR, regardless of
     * the regulator/limit cause underneath. */
    if (s_manual) return APP_MODE_MANUAL;
    power = SystemData_GetPowerStatus();
    if (s_regulator_error || s_limit_error) return APP_MODE_ERROR;
    /* A confirmed eFuse fault remains latched until `fault clear`, but only raises
     * WARNING so software does not shut TRANS down for the FLT indication. */
    if (power.latch_pc4 || power.latch_pc0 || s_pac_unreadable || s_bus_fault ||
        s_setpoint_mismatch || AppMode_LinkLost())
        return APP_MODE_WARNING;
    return APP_MODE_NORMAL;
}

bool AppMode_RegulatorMonitored(void)
{
    return !s_manual && !s_regulator_error;
}

bool AppMode_RequestManual(void)
{
    /* Reachable from ERROR too (decided 2026-10-15): it is the operator's way to take
     * manual control when the automatic retry path is stuck. */
    s_manual = true;
    return true;
}

bool AppMode_RequestNormal(void)
{
    /* Only regulator and limit ERROR causes block the return from MANUAL. An eFuse
     * latch is a WARNING and remains recorded until `fault clear`. */
    if (s_regulator_error || s_limit_error) return false;
    s_manual = false;
    return true;
}

void AppMode_SetRegulatorFault(void)
{
    s_regulator_error = true;
}

void AppMode_SetSetpointMismatch(bool mismatch)
{
    s_setpoint_mismatch = mismatch;
}

bool AppMode_SetpointMismatch(void)
{
    return s_setpoint_mismatch;
}

void AppMode_SetPacUnreadable(bool unreadable)
{
    s_pac_unreadable = unreadable;
}

void AppMode_SetEepromMissing(bool missing)
{
    s_eeprom_missing = missing;
}

void AppMode_SetLimitError(void)
{
    s_limit_error = true;
}

void AppMode_SetBusFault(bool fault)
{
    s_bus_fault = fault;
}

bool AppMode_BusFault(void)
{
    return s_bus_fault;
}

bool AppMode_Retry(bool regulator_ok, bool limits_ok)
{
    if (!regulator_ok || !limits_ok) return false;
    s_regulator_error = false;
    s_limit_error = false;
    return true;
}

bool AppMode_LimitError(void)
{
    return s_limit_error;
}

bool AppMode_RegulatorError(void)
{
    return s_regulator_error;
}

bool AppMode_PacUnreadable(void)
{
    return s_pac_unreadable;
}

bool AppMode_EepromMissing(void)
{
    return s_eeprom_missing;
}

void AppMode_NoteRequest(void)
{
    s_link_seen = true;
    s_last_request_ms = SCH_SystemTick();
}

void AppMode_RestartLinkTimeout(void)
{
    if (s_link_seen) s_last_request_ms = SCH_SystemTick();
}

void AppMode_SetLinkTimeoutMs(uint32_t timeout_ms)
{
    s_link_timeout_ms = timeout_ms;
}

bool AppMode_LinkLost(void)
{
    return s_link_seen &&
           (uint32_t)(SCH_SystemTick() - s_last_request_ms) > s_link_timeout_ms;
}
