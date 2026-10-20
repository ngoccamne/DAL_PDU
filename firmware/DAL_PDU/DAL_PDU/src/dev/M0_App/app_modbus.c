#include "app_modbus.h"

#include <stdint.h>

#include "app_boot.h"
#include "app_clock.h"
#include "app_event.h"
#include "app_config.h"
#include "app_limits.h"
#include "app_mode.h"
#include "app_power.h"
#include "app_retry.h"
#include "../M1_Sys/CommandLine/command.h"
#include "../M1_Sys/Version/version.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M2_BSP/dal_pdu_board.h"
#include "../M2_BSP/Modbus/bsp_modbus.h"
#include "../M2_BSP/Reset/bsp_reset.h"
#include "../M2_BSP/TPS55287/bsp_tps55287.h"
#include "../M4_MiddleWare/ThirdParty/freemodbus/modbus/include/mb.h"

/* Keep coil 00001/00002 and input 10001/10002 aligned with the design map.
 * FreeMODBUS passes one-based addresses to these callbacks. */
#define MB_COIL_VOUT 1U
#define MB_COIL_TRANS 2U
#define MB_COIL_LED 10U
#define MB_COIL_CLEAR_FAULT 3U
#define MB_DISCRETE_FAULT_PC4 1U
#define MB_DISCRETE_FAULT_PC0 2U
#define MB_DISCRETE_LED_ON 10U
#define MB_DISCRETE_TRANS_ON 11U
#define MB_INPUT_STATUS 20U
#define MB_INPUT_BOARD_TEMP 21U
#define MB_INPUT_MODE 22U
#define MB_INPUT_SETPOINT_MV 23U
#define MB_INPUT_RAIL_FIRST 24U   /* VOUT V, VOUT mA, +12 V, +12 mA, +5_TRANS V, mA, +5 V, mA */
#define MB_INPUT_BOOT_COUNT 32U
#define MB_INPUT_BOOT_CAUSE 33U
#define MB_INPUT_BOOT_SOURCE 34U
#define MB_INPUT_POWER_FIRST 35U  /* rail power, VOUT, +12, +5_TRANS, +5, in 10 mW units */
#define MB_INPUT_CLOCK_FIRST 39U    /* 30039-30040: clock now, Unix seconds, high word first */
#define MB_INPUT_CLOCK_LAST 40U
#define MB_INPUT_EVENT_SEQ 41U      /* 30041-30047: the selected event, all zero when none matches */
#define MB_INPUT_EVENT_SEVERITY 42U
#define MB_INPUT_EVENT_CODE 43U
#define MB_INPUT_EVENT_MODE 44U
#define MB_INPUT_EVENT_VALUE 45U
#define MB_INPUT_EVENT_TIME_FIRST 46U /* 30046-30047: event time, high word first */
#define MB_INPUT_CLOCK_SOURCE 48U /* 30048: clock source now, APP_CLOCK_SOURCE_* */
#define MB_INPUT_EVENT_SOURCE 49U /* 30049: clock source of the selected event */
#define MB_INPUT_LAST 49U
#define MB_INPUT_NO_VALUE 0xFFFFU /* unset or unavailable */
#define MB_INPUT_MAX_VALUE 0xFFFEU
#define MB_INPUT_TEMP_INVALID 0x8000U /* no valid reading */
#define MB_HOLDING_VOUT_SETPOINT 1U
#define MB_HOLDING_MODE 30U
#define MB_HOLDING_LIMIT_TEMP 31U
#define MB_HOLDING_LIMIT_VOUT 32U
#define MB_HOLDING_LIMIT_TRANS 33U
#define MB_HOLDING_RETRY_ENABLE 34U
#define MB_HOLDING_RETRY_TIME 35U
#define MB_HOLDING_CLOCK_FIRST 40U  /* 40040-40041: time, Unix seconds, high word first */
#define MB_HOLDING_CLOCK_LAST 41U
#define MB_HOLDING_EVENT_SELECT 50U /* 40050: event sequence to read, 0 is the newest */
#define MB_HOLDING_BOOTLOADER_RESET 60U
#define MB_BOOTLOADER_RESET_KEY 0xB007U
#define MB_BOOTLOADER_RESET_DELAY_MS 100UL /* Reply is at most 8 bytes (67 ms at 1200 baud). */
#define MB_HOLDING_LED_FIRST 20U
#define MB_HOLDING_LED_LAST 23U
#define MB_HOLDING_INFO_FIRST 100U
#define MB_HOLDING_INFO_LAST 109U

static bool s_bootloader_reset_pending;
static uint32_t s_bootloader_reset_requested_ms;

static uint16_t AppModbus_BoardInfoWord(uint16_t address)
{
    BSP_NVM_Config config;
    AppConfig_Get(&config);
    switch (address)
    {
        case 100U: return 0x5044U; /* ASCII "PD" */
        case 101U: return 0x5500U; /* ASCII "U\0" */
        case 102U: return (uint16_t)((FIRMWARE_VERSION_MAJOR << 8) |
                                      FIRMWARE_VERSION_MINOR);
        case 103U: return FIRMWARE_VERSION_PATCH;
        case 104U: return (uint16_t)(F_CPU / 1000000UL);
        case 105U: return config.slave_address;
        case 106U: return (uint16_t)AppConfig_BaudRate(config.baud_code);
        case 107U: return (uint16_t)(FIRMWARE_BUILD_CODE >> 16);
        case 108U: return (uint16_t)FIRMWARE_BUILD_CODE;
        case 109U: return (uint16_t)(AppConfig_BaudRate(config.baud_code) >> 16);
        default: return 0U;
    }
}

static uint16_t AppModbus_HoldingWord(SystemData_HeartbeatTiming timing,
                                      uint16_t address)
{
    uint32_t value = (address <= 21U) ? timing.on_ms : timing.off_ms;
    return (address & 1U) ? (uint16_t)value : (uint16_t)(value >> 16);
}

bool AppModbus_Init(void)
{
    BSP_NVM_Config config;
    s_bootloader_reset_pending = false;
    AppConfig_Get(&config);
    if (eMBInit(MB_RTU, config.slave_address, 1U, AppConfig_BaudRate(config.baud_code),
                MB_PAR_NONE, 1U) != MB_ENOERR)
    {
        return false;
    }
    return eMBEnable() == MB_ENOERR;
}

/* A request needs two polls: the first takes the received frame and posts EV_EXECUTE, the
 * second runs the handler and sends the reply. Called from the 5 ms Modbus task. */
void AppModbus_Update(void)
{
    (void)eMBPoll();
    (void)eMBPoll();
    if (s_bootloader_reset_pending &&
        (uint32_t)(SCH_SystemTick() - s_bootloader_reset_requested_ms) >= MB_BOOTLOADER_RESET_DELAY_MS)
    {
        s_bootloader_reset_pending = false;
        AppPower_TurnOffOutputs();
        BSP_Reset_Bootloader();
    }
}

/* Event selected by holding 40050. Sequence 0 selects the newest event. */
static uint16_t s_event_select;

/* Clock register words. An unset clock reads as 0xFFFFFFFF, the same value as an unset event time. */
static eMBErrorCode AppModbus_Clock(UCHAR *buffer, USHORT address, USHORT count, eMBRegisterMode mode)
{
    uint32_t value;
    uint16_t i;
    if (mode == MB_REG_READ)
    {
        value = AppClock_Now();
        for (i = 0U; i < count; i++)
        {
            uint16_t word = (address + i == MB_HOLDING_CLOCK_FIRST) ? (uint16_t)(value >> 16) : (uint16_t)value;
            buffer[2U * i] = (UCHAR)(word >> 8);
            buffer[2U * i + 1U] = (UCHAR)word;
        }
        return MB_ENOERR;
    }
    /* A time is set with both words, so one word cannot leave the clock half-written. */
    if (address != MB_HOLDING_CLOCK_FIRST || count != 2U) return MB_ENOREG;
    value = ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) |
            ((uint32_t)buffer[2] << 8) | (uint32_t)buffer[3];
    if (!AppClock_Set(value)) return MB_EIO;
    AppEvent_Log(APP_EVENT_SEVERITY_INFO, APP_EVENT_CLOCK_SET, 0);
    return MB_ENOERR;
}

static uint16_t AppModbus_ClockEventWord(uint16_t address, bool found, const AppEvent *event)
{
    uint32_t value;
    if (address == MB_INPUT_CLOCK_SOURCE) return AppClock_Source();
    if (address == MB_INPUT_EVENT_SOURCE) return found ? event->clock_source : 0U;
    if (address <= MB_INPUT_CLOCK_LAST)
    {
        value = AppClock_Now();
        return (address == MB_INPUT_CLOCK_FIRST) ? (uint16_t)(value >> 16) : (uint16_t)value;
    }
    if (!found)
    {
        return (address >= MB_INPUT_EVENT_TIME_FIRST) ? MB_INPUT_NO_VALUE : 0U;
    }
    switch (address)
    {
    case MB_INPUT_EVENT_SEQ: return event->seq;
    case MB_INPUT_EVENT_SEVERITY: return event->severity;
    case MB_INPUT_EVENT_CODE: return event->code;
    case MB_INPUT_EVENT_MODE: return event->mode;
    case MB_INPUT_EVENT_VALUE: return (uint16_t)event->value;
    case MB_INPUT_EVENT_TIME_FIRST: return (uint16_t)(event->time_s >> 16);
    default: return (uint16_t)event->time_s;
    }
}

static eMBErrorCode AppModbus_Limit(UCHAR *buffer, USHORT address, USHORT count, eMBRegisterMode mode)
{
    BSP_NVM_Limits limits;
    uint16_t value;
    if (count != 1U) return MB_ENOREG;
    AppLimits_Get(&limits);
    if (mode == MB_REG_READ)
    {
        if (address == MB_HOLDING_LIMIT_TEMP) value = (uint16_t)limits.temp_trip_tenths_c;
        else if (address == MB_HOLDING_LIMIT_VOUT) value = limits.vout_max_ma;
        else value = limits.trans5v_max_ma;
        buffer[0] = (UCHAR)(value >> 8);
        buffer[1] = (UCHAR)value;
        return MB_ENOERR;
    }
    value = (uint16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
    if (address == MB_HOLDING_LIMIT_TEMP) limits.temp_trip_tenths_c = (int16_t)value;
    else if (address == MB_HOLDING_LIMIT_VOUT) limits.vout_max_ma = value;
    else limits.trans5v_max_ma = value;
    return AppLimits_Set(&limits) ? MB_ENOERR : MB_EIO;
}

eMBErrorCode eMBRegHoldingCB(UCHAR *buffer, USHORT address,
                             USHORT count, eMBRegisterMode mode)
{
    SystemData_HeartbeatTiming timing;
    uint16_t i;
    if (buffer == 0 || count == 0U)
    {
        return MB_ENOREG;
    }
    AppMode_NoteRequest();
    if (address >= MB_HOLDING_CLOCK_FIRST && address <= MB_HOLDING_CLOCK_LAST)
    {
        if (count > MB_HOLDING_CLOCK_LAST + 1U - address) return MB_ENOREG;
        return AppModbus_Clock(buffer, address, count, mode);
    }
    if (address == MB_HOLDING_EVENT_SELECT)
    {
        if (count != 1U) return MB_ENOREG;
        if (mode == MB_REG_READ)
        {
            buffer[0] = (UCHAR)(s_event_select >> 8);
            buffer[1] = (UCHAR)s_event_select;
            return MB_ENOERR;
        }
        s_event_select = (uint16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
        return MB_ENOERR;
    }
    if (address == MB_HOLDING_BOOTLOADER_RESET)
    {
        uint16_t value;
        if (count != 1U) return MB_ENOREG;
        if (mode == MB_REG_READ)
        {
            buffer[0] = 0U;
            buffer[1] = s_bootloader_reset_pending ? 1U : 0U;
            return MB_ENOERR;
        }
        value = (uint16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
        if (value != MB_BOOTLOADER_RESET_KEY) return MB_EIO;
        if (!s_bootloader_reset_pending)
        {
            s_bootloader_reset_requested_ms = SCH_SystemTick();
            s_bootloader_reset_pending = true;
        }
        return MB_ENOERR;
    }
    if (address == MB_HOLDING_MODE)
    {
        bool accepted;
        uint16_t requested;
        if (count != 1U) return MB_ENOREG;
        if (mode == MB_REG_READ)
        {
            buffer[0] = 0U;
            buffer[1] = (UCHAR)AppMode_Get();
            return MB_ENOERR;
        }
        requested = (uint16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
        if (requested == APP_MODE_NORMAL)
        {
            accepted = (!(AppMode_RegulatorError() || AppMode_LimitError()) ||
                        AppLimits_Retry(AppPower_ProbeRegulator())) && AppMode_RequestNormal();
        }
        else if (requested == APP_MODE_MANUAL) accepted = AppMode_RequestManual();
        else if (requested == 4U) accepted = AppRetry_Attempt(); /* write-only retry action */
        else return MB_ENOREG;
        return accepted ? MB_ENOERR : MB_EIO;
    }
    if (address == MB_HOLDING_VOUT_SETPOINT)
    {
        uint16_t voltage;
        if (count != 1U) return MB_ENOREG;
        if (mode == MB_REG_READ)
        {
            BSP_TPS55287_Status tps = BSP_TPS55287_GetStatus();
            voltage = tps.setpoint_valid ? tps.setpoint_mv : (uint16_t)MB_INPUT_NO_VALUE;
            buffer[0] = (UCHAR)(voltage >> 8);
            buffer[1] = (UCHAR)voltage;
        }
        else
        {
            voltage = (uint16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
            if (!AppPower_EnsureRegulatorOn() || !BSP_TPS55287_SetVoltageMv(voltage)) return MB_EIO;
        }
        return MB_ENOERR;
    }
    if (address >= MB_HOLDING_LIMIT_TEMP && address <= MB_HOLDING_LIMIT_TRANS)
    {
        return AppModbus_Limit(buffer, address, count, mode);
    }
    if (address == MB_HOLDING_RETRY_ENABLE || address == MB_HOLDING_RETRY_TIME)
    {
        BSP_NVM_Config config;
        uint16_t value;
        if (count != 1U) return MB_ENOREG;
        AppConfig_Get(&config);
        if (mode == MB_REG_READ)
        {
            value = (address == MB_HOLDING_RETRY_ENABLE) ? config.retry_enable : config.retry_time_s;
            buffer[0] = 0U;
            buffer[1] = (UCHAR)value;
            return MB_ENOERR;
        }
        value = (uint16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
        if (address == MB_HOLDING_RETRY_ENABLE)
        {
            if (value > 1U) return MB_EIO;
            config.retry_enable = (uint8_t)value;
        }
        else
        {
            if (value < APP_CONFIG_RETRY_TIME_MIN_S || value > APP_CONFIG_RETRY_TIME_MAX_S)
                return MB_EIO;
            config.retry_time_s = (uint8_t)value;
        }
        if (!AppConfig_Set(&config)) return MB_EIO;
        AppEvent_Log(APP_EVENT_SEVERITY_INFO, APP_EVENT_CONFIG_CHANGED, 0);
        return MB_ENOERR;
    }
    if (address >= MB_HOLDING_INFO_FIRST && address <= MB_HOLDING_INFO_LAST)
    {
        if (mode != MB_REG_READ || count > MB_HOLDING_INFO_LAST + 1U - address)
            return MB_ENOREG;
        for (i = 0U; i < count; i++)
        {
            uint16_t word = AppModbus_BoardInfoWord((uint16_t)(address + i));
            buffer[2U * i] = (UCHAR)(word >> 8);
            buffer[2U * i + 1U] = (UCHAR)word;
        }
        return MB_ENOERR;
    }
    if (address < MB_HOLDING_LED_FIRST || count > 4U ||
        address > (uint16_t)(MB_HOLDING_LED_LAST + 1U - count)) return MB_ENOREG;
    timing = SystemData_GetHeartbeatTiming();
    for (i = 0U; i < count; i++)
    {
        uint16_t reg = (uint16_t)(address + i);
        uint16_t word;
        if (mode == MB_REG_READ)
        {
            word = AppModbus_HoldingWord(timing, reg);
            buffer[2U * i] = (UCHAR)(word >> 8);
            buffer[2U * i + 1U] = (UCHAR)word;
        }
        else
        {
            word = (uint16_t)(((uint16_t)buffer[2U * i] << 8) |
                              buffer[2U * i + 1U]);
            uint32_t *period = (reg <= 21U) ? &timing.on_ms : &timing.off_ms;
            if (reg & 1U) *period = (*period & 0xFFFF0000UL) | word;
            else *period = (*period & 0x0000FFFFUL) | ((uint32_t)word << 16);
        }
    }
    if (mode == MB_REG_WRITE &&
        !SystemData_SetHeartbeatTiming(timing.on_ms, timing.off_ms))
    {
        return MB_EIO;
    }
    return MB_ENOERR;
}

eMBErrorCode eMBRegCoilsCB(UCHAR *buffer, USHORT address,
                           USHORT count, eMBRegisterMode mode)
{
    SystemData_HeartbeatStatus led;
    SystemData_PowerStatus power;
    bool vout_enabled;
    AppMode_NoteRequest();
    if (buffer == 0 ||
        !((address == MB_COIL_VOUT && (count == 1U || count == 2U)) ||
          (address == MB_COIL_TRANS && count == 1U) ||
          (address == MB_COIL_CLEAR_FAULT && count == 1U) ||
          (address == MB_COIL_LED && count == 1U))) return MB_ENOREG;
    led = SystemData_GetHeartbeatStatus();
    power = SystemData_GetPowerStatus();
    if (mode == MB_REG_READ)
    {
        if (address == MB_COIL_LED)
            *buffer = led.enabled ? 1U : 0U;
        else if (address == MB_COIL_TRANS)
            *buffer = power.trans_on ? 1U : 0U;
        else if (address == MB_COIL_CLEAR_FAULT)
            *buffer = (power.latch_pc4 || power.latch_pc0) ? 1U : 0U;
        else
        {
            if (!BSP_TPS55287_IsEnabled(&vout_enabled)) return MB_EIO;
            *buffer = (uint8_t)((vout_enabled ? 1U : 0U) |
                                (count == 2U && power.trans_on ? 2U : 0U));
        }
    }
    else
    {
        bool enable = (*buffer & 1U) != 0U;
        if (address == MB_COIL_LED) SystemData_SetHeartbeatEnabled(enable);
        else if (address == MB_COIL_TRANS)
        {
            if (!AppPower_SetTrans(enable)) return MB_EIO;
        }
        else if (address == MB_COIL_CLEAR_FAULT)
        {
            if (enable && !AppPower_ClearFaultLatch()) return MB_EIO;
        }
        else if (count == 1U)
        {
            /* Disabling never needs TRANS powered just to turn itself off. */
            if (enable && !AppPower_EnsureRegulatorOn()) return MB_EIO;
            if (!BSP_TPS55287_SetEnabled(enable)) return MB_EIO;
        }
        else
        {
            bool trans_enable = (*buffer & 2U) != 0U;
            if (enable && !trans_enable) return MB_EIO;
            if (!trans_enable)
                return AppPower_SetTrans(false) ? MB_ENOERR : MB_EIO;
            if (trans_enable && !AppPower_SetTrans(true)) return MB_EIO;
            if (!BSP_TPS55287_SetEnabled(enable))
            {
                if (trans_enable && !power.trans_on) (void)AppPower_SetTrans(false);
                return MB_EIO;
            }
        }
    }
    return MB_ENOERR;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *buffer, USHORT address, USHORT count)
{
    SystemData_HeartbeatStatus led;
    SystemData_PowerStatus power;
    uint8_t values;
    AppMode_NoteRequest();
    if (buffer == 0 || count == 0U ||
        !((address >= MB_DISCRETE_FAULT_PC4 &&
           address + count <= MB_DISCRETE_FAULT_PC0 + 1U) ||
          (address >= MB_DISCRETE_LED_ON &&
           address + count <= MB_DISCRETE_TRANS_ON + 1U))) return MB_ENOREG;
    led = SystemData_GetHeartbeatStatus();
    power = SystemData_GetPowerStatus();
    if (address <= MB_DISCRETE_FAULT_PC0)
        values = (uint8_t)((power.fault_pc4 ? 1U : 0U) |
                           (power.fault_pc0 ? 2U : 0U));
    else
        values = (uint8_t)((led.led_on ? 1U : 0U) |
                           (power.trans_on ? 2U : 0U));
    *buffer = (uint8_t)((values >> ((address <= 2U) ? address - 1U : address - 10U)) &
                        ((1U << count) - 1U));
    return MB_ENOERR;
}

static uint16_t AppModbus_TelemetryWord(uint16_t address)
{
    AppBoot_Info boot;
    BSP_TPS55287_Status tps;
    SystemData_PowerMonitorStatus monitor;
    uint16_t offset;
    uint8_t channel;
    uint32_t units;
    if (address >= MB_INPUT_BOOT_COUNT && address <= MB_INPUT_BOOT_SOURCE)
    {
        boot = AppBoot_Get();
        if (address == MB_INPUT_BOOT_COUNT) return boot.boot_count;
        if (address == MB_INPUT_BOOT_CAUSE) return boot.cause;
        return boot.source;
    }
    tps = BSP_TPS55287_GetStatus();
    monitor = SystemData_GetPowerMonitorStatus();
    if (address == MB_INPUT_SETPOINT_MV)
    {
        return tps.setpoint_valid ? tps.setpoint_mv : (uint16_t)MB_INPUT_NO_VALUE;
    }
    if (!monitor.valid) return MB_INPUT_NO_VALUE;
    if (address >= MB_INPUT_POWER_FIRST)
    {
        /* Rail power in 10 mW units, so 100 W fits in one word. */
        channel = (uint8_t)(address - MB_INPUT_POWER_FIRST);
        units = monitor.power_mw[channel] / 10U;
        return (units > MB_INPUT_MAX_VALUE) ? MB_INPUT_MAX_VALUE : (uint16_t)units;
    }
    offset = (uint16_t)(address - MB_INPUT_RAIL_FIRST);
    channel = (uint8_t)(offset / 2U);
    if ((offset & 1U) == 0U) return monitor.voltage_mv[channel];
    if (monitor.current_ma[channel] > MB_INPUT_MAX_VALUE) return MB_INPUT_MAX_VALUE;
    return (uint16_t)monitor.current_ma[channel];
}

eMBErrorCode eMBRegInputCB(UCHAR *buffer, USHORT address, USHORT count)
{
    SystemData_HeartbeatStatus led;
    SystemData_PowerStatus power;
    SystemData_TemperatureStatus temperature;
    AppEvent selected;
    bool selected_found = false;
    uint16_t word;
    uint16_t i;
    if (buffer == 0 || count == 0U ||
        address < MB_INPUT_STATUS ||
        address + count - 1U > MB_INPUT_LAST) return MB_ENOREG;
    AppMode_NoteRequest();
    if (address + count - 1U >= MB_INPUT_EVENT_SEQ) selected_found = AppEvent_Find(s_event_select, &selected);
    led = SystemData_GetHeartbeatStatus();
    power = SystemData_GetPowerStatus();
    temperature = SystemData_GetTemperatureStatus();
    for (i = 0U; i < count; i++)
    {
        if (address + i == MB_INPUT_STATUS)
        {
            word = (uint16_t)((led.led_on ? 1U : 0U) |
                              (power.trans_on ? 2U : 0U) |
                              (power.fault_pc4 ? 4U : 0U) |
                              (power.fault_pc0 ? 8U : 0U) |
                              (power.latch_pc4 ? 0x10U : 0U) |
                              (power.latch_pc0 ? 0x20U : 0U) |
                              (AppMode_SetpointMismatch() ? 0x40U : 0U));
        }
        else if (address + i == MB_INPUT_BOARD_TEMP)
        {
            word = temperature.valid ? (uint16_t)temperature.tenths_c
                                     : MB_INPUT_TEMP_INVALID;
        }
        else if (address + i == MB_INPUT_MODE)
        {
            word = (uint16_t)AppMode_Get();
        }
        else if (address + i >= MB_INPUT_CLOCK_FIRST)
        {
            word = AppModbus_ClockEventWord((uint16_t)(address + i), selected_found, &selected);
        }
        else
        {
            word = AppModbus_TelemetryWord((uint16_t)(address + i));
        }
        buffer[2U * i] = (UCHAR)(word >> 8);
        buffer[2U * i + 1U] = (UCHAR)word;
    }
    return MB_ENOERR;
}
