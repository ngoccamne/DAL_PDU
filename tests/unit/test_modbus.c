#include <stdio.h>
#include <stdint.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_mode.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_boot.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_watchdog.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_modbus.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NVM/bsp_nvm.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Version/version.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/TPS55287/bsp_tps55287.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_clock.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_event.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M4_MiddleWare/ThirdParty/freemodbus/modbus/include/mb.h"

static unsigned failures;
static bool power_allowed = true;
static bool tps_allowed = true;
static bool vout_enabled;
static uint16_t vout_setpoint = 5000U;
static BSP_NVM_Limits fake_limits = {850, 3000U, 3000U};
static bool limits_accept = true;
static uint32_t fake_now = APP_CLOCK_UNSET;
static AppEvent fake_event;
static bool fake_event_found;
static unsigned event_logs;
static uint32_t fake_tick_ms;
static unsigned bootloader_reset_calls;
static bool outputs_off_at_reset;
static uint8_t fake_clock_source;
uint8_t AppClock_Source(void)
{
    return fake_clock_source;
}

uint32_t AppClock_Now(void)
{
    return fake_now;
}

bool AppClock_Set(uint32_t unix_s)
{
    if (unix_s == APP_CLOCK_UNSET) return false;
    fake_now = unix_s;
    return true;
}

bool AppEvent_Find(uint16_t seq, AppEvent *event)
{
    (void)seq;
    if (!fake_event_found) return false;
    *event = fake_event;
    return true;
}

bool AppEvent_Log(uint8_t severity, uint8_t code, int16_t value)
{
    (void)severity;
    (void)code;
    (void)value;
    event_logs++;
    return true;
}
static BSP_TPS55287_Status fake_tps;
BSP_TPS55287_Status BSP_TPS55287_GetStatus(void)
{
    return fake_tps;
}


void AppLimits_Get(BSP_NVM_Limits *limits)
{
    *limits = fake_limits;
}

bool AppLimits_Set(const BSP_NVM_Limits *limits)
{
    if (!limits_accept) return false;
    fake_limits = *limits;
    return true;
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

uint32_t SCH_SystemTick(void) { return fake_tick_ms; }

void AppPower_TurnOffOutputs(void)
{
    SystemData_PowerStatus status = SystemData_GetPowerStatus();
    vout_enabled = false;
    status.trans_on = false;
    SystemData_SetPowerStatus(status);
}

void BSP_Reset_Bootloader(void)
{
    bootloader_reset_calls++;
    outputs_off_at_reset = !vout_enabled && !SystemData_GetPowerStatus().trans_on;
}

static BSP_NVM_Config fake_config = {7U, 4U, 5U, 1U, 5U};
void AppConfig_Get(BSP_NVM_Config *config)
{
    *config = fake_config;
}

bool AppConfig_Set(const BSP_NVM_Config *config)
{
    fake_config = *config;
    return true;
}

bool AppRetry_Attempt(void)
{
    return true;
}

uint32_t AppConfig_BaudRate(uint8_t baud_code)
{
    return baud_code == 4U ? 19200UL : (baud_code == 6U ? 115200UL : 0UL);
}

eMBErrorCode eMBInit(eMBMode mode, UCHAR address, UCHAR port, ULONG baud,
                    eMBParity parity, UCHAR stops)
{
    CHECK(mode == MB_RTU && address == 7U && port == 1U);
    CHECK(baud == 19200UL && parity == MB_PAR_NONE && stops == 1U);
    return MB_ENOERR;
}
eMBErrorCode eMBEnable(void) { return MB_ENOERR; }
eMBErrorCode eMBPoll(void) { return MB_ENOERR; }

static AppMode fake_mode = APP_MODE_WARNING;
AppMode AppMode_Get(void)
{
    return fake_mode;
}

void AppMode_NoteRequest(void)
{
}

bool AppMode_RequestManual(void)
{
    return fake_mode != APP_MODE_ERROR;
}

bool AppMode_RequestNormal(void)
{
    return fake_mode != APP_MODE_ERROR;
}

bool AppMode_RegulatorError(void)
{
    return fake_mode == APP_MODE_ERROR;
}

bool AppMode_LimitError(void)
{
    return false;
}

bool AppPower_ProbeRegulator(void)
{
    return true;
}

bool AppLimits_Retry(bool regulator_ok)
{
    if (regulator_ok && fake_mode == APP_MODE_ERROR) fake_mode = APP_MODE_WARNING;
    return regulator_ok;
}

bool AppPower_ClearFaultLatch(void)
{
    return true;
}

bool AppPower_SetTrans(bool enable)
{
    SystemData_PowerStatus status = SystemData_GetPowerStatus();
    if (enable && !power_allowed) return false;
    if (!enable) vout_enabled = false;
    status.trans_on = enable;
    SystemData_SetPowerStatus(status);
    return true;
}

bool AppPower_EnsureRegulatorOn(void)
{
    return SystemData_GetPowerStatus().trans_on ? true : AppPower_SetTrans(true);
}

bool BSP_TPS55287_SetEnabled(bool enable)
{
    if (!tps_allowed || (enable && !SystemData_GetPowerStatus().trans_on)) return false;
    vout_enabled = enable;
    return true;
}

bool BSP_TPS55287_IsEnabled(bool *enabled)
{
    if (!tps_allowed || enabled == 0) return false;
    *enabled = vout_enabled;
    return true;
}

bool BSP_TPS55287_GetVoltageMv(uint16_t *millivolts)
{
    if (!tps_allowed || millivolts == 0) return false;
    *millivolts = vout_setpoint;
    return true;
}

bool BSP_TPS55287_SetVoltageMv(uint16_t millivolts)
{
    if (!tps_allowed || millivolts < 800U || millivolts > 20000U) return false;
    vout_setpoint = millivolts;
    {
        BSP_TPS55287_Status tps = fake_tps;
        tps.setpoint_valid = true;
        tps.setpoint_mv = millivolts;
        fake_tps = tps;
    }
    return true;
}

bool AppMode_SetpointMismatch(void)
{
    return false;
}

AppBoot_Info AppBoot_Get(void)
{
    AppBoot_Info info = {7U, APP_BOOT_CAUSE_CLIENT_WATCHDOG, APP_WATCHDOG_SOURCE_MODBUS};
    return info;
}

int main(void)
{
    UCHAR bytes[16] = {0};
    SystemData_HeartbeatTiming timing;
    SystemData_PowerStatus power = {false, true, false, false, false};
    SystemData_Init();
    CHECK(AppModbus_Init());
    CHECK(eMBRegHoldingCB(bytes, 1U, 1U, MB_REG_READ) == MB_ENOERR);
    CHECK(bytes[0] == 0xFFU && bytes[1] == 0xFFU); /* Unset until commanded. */
    bytes[0] = 0x13U; bytes[1] = 0xECU;
    CHECK(eMBRegHoldingCB(bytes, 1U, 1U, MB_REG_WRITE) == MB_ENOERR);
    CHECK(vout_setpoint == 5100U);
    {
        UCHAR readback[2] = {0U, 0U};
        CHECK(eMBRegHoldingCB(readback, 1U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(readback[0] == 0x13U && readback[1] == 0xECU); /* 5100 mV from system data. */
    }
    CHECK(eMBRegHoldingCB(bytes, 1U, 2U, MB_REG_READ) == MB_ENOREG);
    CHECK(eMBRegHoldingCB(bytes, 100U, 7U, MB_REG_READ) == MB_ENOERR);
    CHECK(bytes[0] == 'P' && bytes[1] == 'D' && bytes[2] == 'U' && bytes[3] == 0U);
    CHECK(bytes[4] == 0U && bytes[5] == 1U && bytes[6] == 0U && bytes[7] == 0U);
    CHECK(bytes[8] == 0U && bytes[9] == 12U); /* CPU MHz. */
    CHECK(bytes[10] == 0U && bytes[11] == 7U); /* RTU slave address, from the configuration. */
    CHECK(bytes[12] == 0x4BU && bytes[13] == 0x00U); /* 19200 baud, from the configuration. */
    CHECK(eMBRegHoldingCB(bytes, 100U, 1U, MB_REG_WRITE) == MB_ENOREG);
    CHECK(eMBRegHoldingCB(bytes, 109U, 1U, MB_REG_READ) == MB_ENOERR);
    CHECK(bytes[0] == 0U && bytes[1] == 0U); /* High baud word at 19200. */
    fake_config.baud_code = 6U;
    CHECK(eMBRegHoldingCB(bytes, 106U, 1U, MB_REG_READ) == MB_ENOERR);
    CHECK(bytes[0] == 0xC2U && bytes[1] == 0U); /* Low word of 115200. */
    CHECK(eMBRegHoldingCB(bytes, 109U, 1U, MB_REG_READ) == MB_ENOERR);
    CHECK(bytes[0] == 0U && bytes[1] == 1U); /* High word of 115200. */
    fake_config.baud_code = 4U;
    CHECK(eMBRegHoldingCB(bytes, 110U, 1U, MB_REG_READ) == MB_ENOREG);
    CHECK(eMBRegHoldingCB(bytes, 107U, 2U, MB_REG_READ) == MB_ENOERR);
    CHECK(bytes[0] == (UCHAR)(FIRMWARE_BUILD_CODE >> 24) && bytes[1] == (UCHAR)(FIRMWARE_BUILD_CODE >> 16));
    CHECK(bytes[2] == (UCHAR)(FIRMWARE_BUILD_CODE >> 8) && bytes[3] == (UCHAR)FIRMWARE_BUILD_CODE);
    bytes[0] = 0U; bytes[1] = 1U;
    CHECK(eMBRegHoldingCB(bytes, 1U, 1U, MB_REG_WRITE) == MB_EIO);
    CHECK(vout_setpoint == 5100U);
    CHECK(eMBRegHoldingCB(bytes, 20U, 4U, MB_REG_READ) == MB_ENOERR);
    CHECK(bytes[0] == 0U && bytes[1] == 0U && bytes[2] == 3U && bytes[3] == 232U);
    bytes[0] = 0U; bytes[1] = 0U; bytes[2] = 1U; bytes[3] = 244U;
    bytes[4] = 0U; bytes[5] = 0U; bytes[6] = 3U; bytes[7] = 232U;
    CHECK(eMBRegHoldingCB(bytes, 20U, 4U, MB_REG_WRITE) == MB_ENOERR);
    timing = SystemData_GetHeartbeatTiming();
    CHECK(timing.on_ms == 500UL && timing.off_ms == 1000UL);
    CHECK(eMBRegHoldingCB(bytes, 23U, 2U, MB_REG_READ) == MB_ENOREG);
    bytes[0] = 0U; bytes[1] = 0U;
    CHECK(eMBRegHoldingCB(bytes, 21U, 1U, MB_REG_WRITE) == MB_EIO);
    CHECK(SystemData_GetHeartbeatTiming().on_ms == 500UL);

    bytes[0] = 0U;
    CHECK(eMBRegCoilsCB(bytes, 10U, 1U, MB_REG_WRITE) == MB_ENOERR);
    CHECK(!SystemData_GetHeartbeatStatus().enabled);
    bytes[0] = 1U;
    CHECK(eMBRegCoilsCB(bytes, 2U, 1U, MB_REG_WRITE) == MB_ENOERR);
    CHECK(SystemData_GetPowerStatus().trans_on);
    CHECK(eMBRegCoilsCB(bytes, 1U, 1U, MB_REG_WRITE) == MB_ENOERR);
    CHECK(vout_enabled);
    bytes[0] = 0U;
    CHECK(eMBRegCoilsCB(bytes, 1U, 2U, MB_REG_READ) == MB_ENOERR);
    CHECK(bytes[0] == 3U);
    bytes[0] = 0U;
    CHECK(eMBRegCoilsCB(bytes, 1U, 2U, MB_REG_WRITE) == MB_ENOERR);
    CHECK(!vout_enabled && !SystemData_GetPowerStatus().trans_on);
    tps_allowed = false;
    bytes[0] = 3U;
    CHECK(eMBRegCoilsCB(bytes, 1U, 2U, MB_REG_WRITE) == MB_EIO);
    CHECK(!SystemData_GetPowerStatus().trans_on); /* Roll back new TRANS enable. */
    tps_allowed = true;
    bytes[0] = 1U;
    CHECK(eMBRegCoilsCB(bytes, 1U, 2U, MB_REG_WRITE) == MB_EIO);
    CHECK(!SystemData_GetPowerStatus().trans_on);
    bytes[0] = 3U;
    CHECK(eMBRegCoilsCB(bytes, 1U, 2U, MB_REG_WRITE) == MB_ENOERR);
    CHECK(vout_enabled && SystemData_GetPowerStatus().trans_on);
    tps_allowed = false;
    CHECK(eMBRegCoilsCB(bytes, 1U, 2U, MB_REG_READ) == MB_EIO);
    tps_allowed = true;
    power_allowed = false;
    CHECK(eMBRegCoilsCB(bytes, 2U, 1U, MB_REG_WRITE) == MB_EIO);
    power.trans_on = true;
    SystemData_SetPowerStatus(power);
    SystemData_SetHeartbeatLedOn(true);
    CHECK(eMBRegDiscreteCB(bytes, 1U, 2U) == MB_ENOERR);
    CHECK(bytes[0] == 1U);
    CHECK(eMBRegDiscreteCB(bytes, 10U, 2U) == MB_ENOERR);
    CHECK(bytes[0] == 3U);
    CHECK(eMBRegInputCB(bytes, 20U, 1U) == MB_ENOERR);
    CHECK(bytes[0] == 0U && bytes[1] == 7U);
    {
        SystemData_TemperatureStatus temperature = {true, 251};
        SystemData_SetTemperatureStatus(temperature);
        CHECK(eMBRegInputCB(bytes, 21U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x00U && bytes[1] == 0xFBU);
        CHECK(eMBRegInputCB(bytes, 20U, 2U) == MB_ENOERR);
        CHECK(bytes[1] == 7U && bytes[2] == 0x00U && bytes[3] == 0xFBU);
        temperature.valid = false;
        SystemData_SetTemperatureStatus(temperature);
        CHECK(eMBRegInputCB(bytes, 21U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x80U && bytes[1] == 0x00U);
        CHECK(eMBRegInputCB(bytes, 22U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x00U && bytes[1] == (uint8_t)APP_MODE_WARNING);
        CHECK(eMBRegInputCB(bytes, 23U, 1U) == MB_ENOERR);
    }
    {
        UCHAR mode_bytes[2] = {0x00U, 0x01U};
        CHECK(eMBRegHoldingCB(mode_bytes, 30U, 1U, MB_REG_WRITE) == MB_ENOERR);
        mode_bytes[1] = 0x02U;
        CHECK(eMBRegHoldingCB(mode_bytes, 30U, 1U, MB_REG_WRITE) == MB_ENOREG);
        CHECK(eMBRegHoldingCB(mode_bytes, 30U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(mode_bytes[1] == (UCHAR)APP_MODE_WARNING);
        fake_mode = APP_MODE_ERROR;
        mode_bytes[1] = 0x00U;
        CHECK(eMBRegHoldingCB(mode_bytes, 30U, 1U, MB_REG_WRITE) == MB_ENOERR);
        CHECK(fake_mode == APP_MODE_WARNING); /* Normal request checks and clears ERROR. */
        mode_bytes[1] = 0x04U;
        CHECK(eMBRegHoldingCB(mode_bytes, 30U, 1U, MB_REG_WRITE) == MB_ENOERR);
        fake_mode = APP_MODE_WARNING;
    }
    {
        UCHAR limit_bytes[2] = {0x03U, 0x52U};
        CHECK(eMBRegHoldingCB(limit_bytes, 31U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(limit_bytes[0] == 0x03U && limit_bytes[1] == 0x52U);
        limit_bytes[0] = 0x03U;
        limit_bytes[1] = 0x84U;
        CHECK(eMBRegHoldingCB(limit_bytes, 31U, 1U, MB_REG_WRITE) == MB_ENOERR);
        CHECK(fake_limits.temp_trip_tenths_c == 900);
        limit_bytes[0] = 0x0BU;
        limit_bytes[1] = 0xB8U;
        CHECK(eMBRegHoldingCB(limit_bytes, 32U, 1U, MB_REG_WRITE) == MB_ENOERR);
        CHECK(fake_limits.vout_max_ma == 3000U);
        limits_accept = false;
        CHECK(eMBRegHoldingCB(limit_bytes, 33U, 1U, MB_REG_WRITE) == MB_EIO);
        limits_accept = true;
        CHECK(eMBRegHoldingCB(limit_bytes, 31U, 2U, MB_REG_READ) == MB_ENOREG);
        CHECK(eMBRegHoldingCB(limit_bytes, 36U, 1U, MB_REG_READ) == MB_ENOREG);
    }
    {
        UCHAR retry_bytes[2] = {0U, 0U};
        CHECK(eMBRegHoldingCB(retry_bytes, 34U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(retry_bytes[1] == 1U);
        retry_bytes[1] = 0U;
        CHECK(eMBRegHoldingCB(retry_bytes, 34U, 1U, MB_REG_WRITE) == MB_ENOERR);
        CHECK(fake_config.retry_enable == 0U);
        retry_bytes[1] = 12U;
        CHECK(eMBRegHoldingCB(retry_bytes, 35U, 1U, MB_REG_WRITE) == MB_ENOERR);
        CHECK(fake_config.retry_time_s == 12U);
        CHECK(event_logs == 2U);
        CHECK(eMBRegHoldingCB(retry_bytes, 35U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(retry_bytes[1] == 12U);
        retry_bytes[1] = 0U;
        CHECK(eMBRegHoldingCB(retry_bytes, 35U, 1U, MB_REG_WRITE) == MB_EIO);
        retry_bytes[1] = 2U;
        CHECK(eMBRegHoldingCB(retry_bytes, 34U, 1U, MB_REG_WRITE) == MB_EIO);
        CHECK(eMBRegHoldingCB(retry_bytes, 34U, 2U, MB_REG_READ) == MB_ENOREG);
    }
    {
        BSP_TPS55287_Status tps = fake_tps;
        SystemData_PowerMonitorStatus monitor;
        tps.setpoint_valid = true;
        tps.setpoint_mv = 5000U;
        fake_tps = tps;
        CHECK(eMBRegInputCB(bytes, 23U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x13U && bytes[1] == 0x88U);
        monitor = SystemData_GetPowerMonitorStatus();
        monitor.valid = true;
        monitor.voltage_mv[0] = 5012U;
        monitor.current_ma[0] = 1003U;
        monitor.power_mw[0] = 12340U;
        monitor.power_mw[3] = 1000000U;
        SystemData_SetPowerMonitorStatus(monitor);
        CHECK(eMBRegInputCB(bytes, 24U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x13U && bytes[1] == 0x94U);
        CHECK(eMBRegInputCB(bytes, 25U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x03U && bytes[1] == 0xEBU);
        CHECK(eMBRegInputCB(bytes, 35U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x04U && bytes[1] == 0xD2U); /* 12340 mW, in 10 mW units: 1234. */
        CHECK(eMBRegInputCB(bytes, 38U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0xFFU && bytes[1] == 0xFEU); /* Clamped to 0xFFFE. */
        monitor.valid = false;
        SystemData_SetPowerMonitorStatus(monitor);
        CHECK(eMBRegInputCB(bytes, 24U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0xFFU && bytes[1] == 0xFFU);
        CHECK(eMBRegInputCB(bytes, 35U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0xFFU && bytes[1] == 0xFFU); /* Power unavailable with the PAC1934. */
        CHECK(eMBRegInputCB(bytes, 31U, 20U) == MB_ENOREG);
        CHECK(eMBRegInputCB(bytes, 32U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x00U && bytes[1] == 0x07U); /* Boot count. */
        CHECK(eMBRegInputCB(bytes, 33U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x00U && bytes[1] == 0x05U); /* Boot cause: client watchdog. */
        CHECK(eMBRegInputCB(bytes, 34U, 1U) == MB_ENOERR);
        CHECK(bytes[0] == 0x00U && bytes[1] == 0x03U); /* Watchdog source: Modbus. */
        CHECK(eMBRegInputCB(bytes, 50U, 1U) == MB_ENOREG);
    }
    {
        UCHAR time_bytes[4] = {0x5FU, 0x00U, 0x00U, 0x00U};
        CHECK(eMBRegInputCB(bytes, 39U, 2U) == MB_ENOERR);
        CHECK(bytes[0] == 0xFFU && bytes[1] == 0xFFU && bytes[2] == 0xFFU && bytes[3] == 0xFFU); /* Unset. */
        CHECK(eMBRegHoldingCB(bytes, 40U, 2U, MB_REG_READ) == MB_ENOERR);
        CHECK(bytes[0] == 0xFFU && bytes[3] == 0xFFU);
        CHECK(eMBRegHoldingCB(time_bytes, 40U, 1U, MB_REG_WRITE) == MB_ENOREG); /* Both words or none. */
        CHECK(eMBRegHoldingCB(time_bytes, 40U, 2U, MB_REG_WRITE) == MB_ENOERR);
        CHECK(fake_now == 0x5F000000UL && event_logs == 3U);
        CHECK(eMBRegInputCB(bytes, 39U, 2U) == MB_ENOERR);
        CHECK(bytes[0] == 0x5FU && bytes[1] == 0x00U && bytes[2] == 0x00U && bytes[3] == 0x00U);
        CHECK(eMBRegHoldingCB(bytes, 41U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(bytes[0] == 0x00U && bytes[1] == 0x00U); /* Low word. */
        time_bytes[0] = 0xFFU; time_bytes[1] = 0xFFU; time_bytes[2] = 0xFFU; time_bytes[3] = 0xFFU;
        CHECK(eMBRegHoldingCB(time_bytes, 40U, 2U, MB_REG_WRITE) == MB_EIO); /* Reserved value. */
        CHECK(fake_now == 0x5F000000UL);
    }
    {
        UCHAR select[2] = {0x00U, 0x07U};
        UCHAR fields[14];
        fake_event.seq = 7U;
        fake_event.severity = 2U;
        fake_event.code = 3U;
        fake_event.mode = 1U;
        fake_event.value = -5;
        fake_event.time_s = 0x12345678UL;
        fake_event_found = true;
        CHECK(eMBRegHoldingCB(select, 50U, 1U, MB_REG_WRITE) == MB_ENOERR);
        CHECK(eMBRegHoldingCB(select, 50U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(select[0] == 0x00U && select[1] == 0x07U);
        CHECK(eMBRegInputCB(fields, 41U, 7U) == MB_ENOERR);
        CHECK(fields[0] == 0x00U && fields[1] == 0x07U); /* Sequence. */
        CHECK(fields[2] == 0x00U && fields[3] == 0x02U); /* Severity. */
        CHECK(fields[4] == 0x00U && fields[5] == 0x03U); /* Code. */
        CHECK(fields[6] == 0x00U && fields[7] == 0x01U); /* Mode. */
        CHECK(fields[8] == 0xFFU && fields[9] == 0xFBU); /* Value, -5. */
        CHECK(fields[10] == 0x12U && fields[11] == 0x34U && fields[12] == 0x56U && fields[13] == 0x78U);
        fake_event_found = false;
        CHECK(eMBRegInputCB(fields, 41U, 7U) == MB_ENOERR);
        CHECK(fields[0] == 0x00U && fields[1] == 0x00U); /* No event: sequence 0. */
        CHECK(fields[12] == 0xFFU && fields[13] == 0xFFU);
    }
    {
        UCHAR sources[4];
        fake_clock_source = 1U;
        fake_event.clock_source = 2U;
        fake_event_found = true;
        CHECK(eMBRegInputCB(sources, 48U, 2U) == MB_ENOERR);
        CHECK(sources[0] == 0x00U && sources[1] == 0x01U); /* Clock now: restored. */
        CHECK(sources[2] == 0x00U && sources[3] == 0x02U); /* Selected event: set. */
    }
    {
        UCHAR command[2] = {0xB0U, 0x07U};
        UCHAR invalid[2] = {0U, 1U};
        CHECK(eMBRegHoldingCB(invalid, 60U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(invalid[0] == 0U && invalid[1] == 0U);
        CHECK(eMBRegHoldingCB(invalid, 60U, 1U, MB_REG_WRITE) == MB_EIO);
        CHECK(eMBRegHoldingCB(command, 60U, 2U, MB_REG_WRITE) == MB_ENOREG);
        CHECK(bootloader_reset_calls == 0U);
        power_allowed = true;
        CHECK(AppPower_SetTrans(true));
        vout_enabled = true;
        fake_tick_ms = 1000U;
        CHECK(eMBRegHoldingCB(command, 60U, 1U, MB_REG_WRITE) == MB_ENOERR);
        CHECK(eMBRegHoldingCB(invalid, 60U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(invalid[1] == 1U);
        fake_tick_ms = 1099U;
        AppModbus_Update();
        CHECK(bootloader_reset_calls == 0U && vout_enabled);
        fake_tick_ms = 1100U;
        AppModbus_Update();
        CHECK(bootloader_reset_calls == 1U && outputs_off_at_reset);
        CHECK(eMBRegHoldingCB(invalid, 60U, 1U, MB_REG_READ) == MB_ENOERR);
        CHECK(invalid[1] == 0U);
        AppModbus_Update();
        CHECK(bootloader_reset_calls == 1U);
    }
    CHECK(eMBRegDiscreteCB(bytes, 3U, 1U) == MB_ENOREG);
    return failures ? 1 : 0;
}
