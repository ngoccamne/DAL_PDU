#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_event.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_limits.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_mode.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Scheduler/scheduler.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NVM/bsp_nvm.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/cmdline.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_event.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_fault.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_limit.h"

static unsigned failures;

/* Console capture for the limit and event commands. */
static char console[512];
static size_t console_len;

void Command_OutputString(CommandContext *context, const char *text)
{
    size_t n = strlen(text);
    (void)context;
    if (console_len + n >= sizeof(console)) n = sizeof(console) - 1U - console_len;
    memcpy(&console[console_len], text, n);
    console_len += n;
    console[console_len] = '\0';
}

static int Run(int (*command)(CommandContext *, int, char **), char **args, int argc)
{
    CommandContext context;
    memset(&context, 0, sizeof(context));
    console_len = 0U;
    console[0] = '\0';
    return command(&context, argc, args);
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

/* 24C04 stand-in. */
static uint8_t eeprom[512];

/* Internal EEPROM limit record stand-in. */
static BSP_NVM_Limits nvm_limits;
static bool nvm_valid;
static unsigned nvm_writes;

static unsigned turn_off_calls;
static bool nvm_limit_error;
static bool regulator_present;
static bool efuse_active;
static bool pac_present;
static bool eeprom_present;
static bool bus_fault;
static bool setpoint_mismatch;

bool BSP_NVM_ReadLimitError(void)
{
    return nvm_limit_error;
}

void BSP_NVM_WriteLimitError(bool set)
{
    nvm_limit_error = set;
}

bool BSP_EEPROM_Read(uint16_t address, uint8_t *data, uint16_t length)
{
    if ((uint32_t)address + length > sizeof(eeprom)) return false;
    memcpy(data, &eeprom[address], length);
    return true;
}

bool BSP_EEPROM_Write(uint16_t address, const uint8_t *data, uint16_t length)
{
    if ((uint32_t)address + length > sizeof(eeprom)) return false;
    memcpy(&eeprom[address], data, length);
    return true;
}

void AppPower_TurnOffOutputs(void)
{
    turn_off_calls++;
}

void AppRetry_NoteShutdown(void)
{
}

bool AppPower_ProbeRegulator(void)
{
    return regulator_present;
}

bool AppPower_ClearFaultLatch(void)
{
    return !efuse_active;
}

void AppPowerMonitor_Update(void)
{
    AppMode_SetPacUnreadable(!pac_present);
    AppMode_SetSetpointMismatch(setpoint_mismatch);
}

bool BSP_EEPROM_Probe(void)
{
    return eeprom_present;
}

bool BSP_I2C_BusFault(void)
{
    return bus_fault;
}

bool BSP_NVM_ReadLimits(BSP_NVM_Limits *limits)
{
    if (!nvm_valid) return false;
    *limits = nvm_limits;
    return true;
}

void BSP_NVM_WriteLimits(const BSP_NVM_Limits *limits)
{
    nvm_limits = *limits;
    nvm_valid = true;
    nvm_writes++;
}

static void set_temperature(bool valid, int16_t tenths_c)
{
    SystemData_TemperatureStatus status = {valid, tenths_c};
    SystemData_SetTemperatureStatus(status);
}

static void set_currents(uint32_t vout_ma, uint32_t trans_ma)
{
    SystemData_PowerMonitorStatus status;
    memset(&status, 0, sizeof(status));
    status.valid = true;
    status.current_ma[0] = vout_ma;
    status.current_ma[2] = trans_ma;
    SystemData_SetPowerMonitorStatus(status);
}

static void reset_fixture(void)
{
    SystemData_Init();
    SCH_Initialize();
    memset(eeprom, 0xFF, sizeof(eeprom));
    nvm_valid = false;
    nvm_writes = 0U;
    nvm_limit_error = false;
    regulator_present = true;
    efuse_active = false;
    pac_present = true;
    eeprom_present = true;
    bus_fault = false;
    setpoint_mismatch = false;
    turn_off_calls = 0U;
    AppLimits_Init();
    AppMode_Retry(true, true);
    AppMode_SelfTest(true, true, true);
    AppMode_RequestNormal();
    set_temperature(true, 250);
    set_currents(0U, 0U);
    AppLimits_Update();
}

static void test_defaults_when_record_blank(void)
{
    BSP_NVM_Limits limits;
    reset_fixture();
    AppLimits_Get(&limits);
    CHECK(limits.temp_trip_tenths_c == APP_LIMITS_TEMP_DEFAULT_TENTHS_C);
    CHECK(limits.vout_max_ma == APP_LIMITS_VOUT_DEFAULT_MA);
    CHECK(limits.trans5v_max_ma == APP_LIMITS_TRANS5V_DEFAULT_MA);
}

static void test_loads_valid_record(void)
{
    BSP_NVM_Limits limits;
    reset_fixture();
    nvm_limits.temp_trip_tenths_c = 600;
    nvm_limits.vout_max_ma = 2000U;
    nvm_limits.trans5v_max_ma = 1500U;
    nvm_valid = true;
    AppLimits_Init();
    AppLimits_Get(&limits);
    CHECK(limits.temp_trip_tenths_c == 600);
    CHECK(limits.vout_max_ma == 2000U);
    CHECK(limits.trans5v_max_ma == 1500U);
}

static void test_set_validates_and_persists(void)
{
    BSP_NVM_Limits limits;
    reset_fixture();
    AppLimits_Get(&limits);

    limits.temp_trip_tenths_c = 1251;
    CHECK(!AppLimits_Set(&limits));
    limits.temp_trip_tenths_c = -401;
    CHECK(!AppLimits_Set(&limits));
    limits.temp_trip_tenths_c = 850;
    limits.vout_max_ma = 99U;
    CHECK(!AppLimits_Set(&limits));
    limits.vout_max_ma = 3000U;
    limits.trans5v_max_ma = 5001U;
    CHECK(!AppLimits_Set(&limits));
    CHECK(nvm_writes == 0U);

    limits.trans5v_max_ma = 5000U;
    limits.temp_trip_tenths_c = -400;
    limits.vout_max_ma = 100U;
    CHECK(AppLimits_Set(&limits));
    CHECK(nvm_writes == 1U && nvm_valid);
    CHECK(nvm_limits.temp_trip_tenths_c == -400 && nvm_limits.vout_max_ma == 100U);

    limits.temp_trip_tenths_c = 1250;
    CHECK(AppLimits_Set(&limits));
    CHECK(nvm_limits.temp_trip_tenths_c == 1250);
}

static void test_temperature_trips_in_normal(void)
{
    AppEvent event;
    reset_fixture();
    set_temperature(true, APP_LIMITS_TEMP_DEFAULT_TENTHS_C);
    AppLimits_Update();
    CHECK(turn_off_calls == 0U && AppMode_Get() == APP_MODE_NORMAL);
    AppLimits_Update();
    CHECK(turn_off_calls == 1U);
    CHECK(AppMode_Get() == APP_MODE_ERROR && AppMode_LimitError());
    CHECK(AppEvent_Read(0U, &event));
    CHECK(event.severity == APP_EVENT_SEVERITY_WARNING);
    CHECK(event.code == APP_EVENT_TEMP_LIMIT);
    CHECK(event.value == APP_LIMITS_TEMP_DEFAULT_TENTHS_C);
    CHECK(event.mode == APP_MODE_NORMAL);
    CHECK(event.seq == 1U);
}

static void test_trip_logged_once_per_excursion(void)
{
    AppEvent event;
    reset_fixture();
    set_temperature(true, 900);
    AppLimits_Update();
    AppLimits_Update();
    AppLimits_Update();
    AppLimits_Update();
    CHECK(turn_off_calls == 1U);
    CHECK(!AppEvent_Read(1U, &event));

    set_temperature(true, 250);
    AppLimits_Update();
    set_temperature(true, 900);
    AppLimits_Update();
    AppLimits_Update();
    CHECK(AppEvent_Read(1U, &event));
    CHECK(event.seq == 2U);
}

static void test_trip_in_manual_logs_without_acting(void)
{
    AppEvent event;
    reset_fixture();
    CHECK(AppMode_RequestManual());
    set_temperature(true, 900);
    AppLimits_Update();
    AppLimits_Update();
    CHECK(turn_off_calls == 0U);
    CHECK(AppMode_Get() == APP_MODE_MANUAL && !AppMode_LimitError());
    CHECK(AppEvent_Read(0U, &event));
    CHECK(event.code == APP_EVENT_TEMP_LIMIT && event.mode == APP_MODE_MANUAL);
}

static void test_current_trips_in_normal(void)
{
    AppEvent event;
    reset_fixture();
    set_currents(APP_LIMITS_VOUT_DEFAULT_MA + 1U, 0U);
    AppLimits_Update();
    AppLimits_Update();
    CHECK(turn_off_calls == 0U);
    AppLimits_Update();
    CHECK(turn_off_calls == 1U && AppMode_Get() == APP_MODE_ERROR);
    CHECK(AppEvent_Read(0U, &event));
    CHECK(event.code == APP_EVENT_VOUT_CURRENT);
    CHECK(event.value == (int16_t)(APP_LIMITS_VOUT_DEFAULT_MA + 1U));
}

static void test_trans_current_trips_in_normal(void)
{
    AppEvent event;
    reset_fixture();
    set_currents(0U, APP_LIMITS_TRANS5V_DEFAULT_MA + 1U);
    AppLimits_Update();
    AppLimits_Update();
    AppLimits_Update();
    CHECK(turn_off_calls == 1U && AppMode_Get() == APP_MODE_ERROR);
    CHECK(AppEvent_Read(0U, &event));
    CHECK(event.code == APP_EVENT_TRANS_CURRENT);
}

static void test_single_sample_and_invalid_do_not_trip(void)
{
    reset_fixture();
    set_temperature(true, 900);
    AppLimits_Update();
    set_temperature(true, 250);
    AppLimits_Update();
    AppLimits_Update();
    CHECK(turn_off_calls == 0U && AppMode_Get() == APP_MODE_NORMAL);

    set_temperature(false, 2000);
    AppLimits_Update();
    AppLimits_Update();
    AppLimits_Update();
    CHECK(turn_off_calls == 0U && AppMode_Get() == APP_MODE_NORMAL);
}

static void test_retry_refused_while_limit_active(void)
{
    reset_fixture();
    set_temperature(true, 900);
    AppLimits_Update();
    AppLimits_Update();
    CHECK(AppMode_Get() == APP_MODE_ERROR);
    CHECK(!AppMode_Retry(true, !AppLimits_Active()));
    CHECK(AppMode_Get() == APP_MODE_ERROR);

    set_temperature(true, 250);
    CHECK(AppMode_Retry(true, !AppLimits_Active()));
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
}

static void test_active_reports_live_state(void)
{
    reset_fixture();
    set_temperature(true, 900);
    set_currents(APP_LIMITS_VOUT_DEFAULT_MA + 1U, 0U);
    CHECK(AppLimits_Active());
    set_temperature(true, 250);
    CHECK(AppLimits_Active());
    set_currents(0U, 0U);
    CHECK(!AppLimits_Active());
}

static void test_event_ring_wraps(void)
{
    AppEvent event;
    unsigned i;
    reset_fixture();
    for (i = 0U; i < 33U; i++)
    {
        CHECK(AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_TEMP_LIMIT, (int16_t)i));
    }
    CHECK(AppEvent_Read(0U, &event) && event.seq == 33U && event.value == 32);
    CHECK(AppEvent_Read(1U, &event) && event.seq == 2U);
    CHECK(AppEvent_Read(31U, &event) && event.seq == 32U);
}

static void test_limit_command(void)
{
    char *get[] = {"limit"};
    char *set_temp[] = {"limit", "temp", "900"};
    char *over_range[] = {"limit", "vout", "70000"};
    char *negative[] = {"limit", "temp", "-500"};
    char *bad_name[] = {"limit", "foo", "5"};

    reset_fixture();
    CHECK(Run(Cmd_limit, get, 1) == CMDLINE_OK);
    CHECK(strcmp(console, "LIMIT temp=850 vout=1000 trans=1000\r\n") == 0);
    CHECK(Run(Cmd_limit, set_temp, 3) == CMDLINE_OK);
    CHECK(strcmp(console, "LIMIT temp=900 vout=1000 trans=1000\r\n") == 0);
    /* 70000 must be refused, not wrapped into the accepted range. */
    CHECK(Run(Cmd_limit, over_range, 3) == CMDLINE_OK);
    CHECK(strcmp(console, "LIMIT refused: out of range\r\n") == 0);
    CHECK(Run(Cmd_limit, negative, 3) == CMDLINE_OK);
    CHECK(strcmp(console, "LIMIT refused: out of range\r\n") == 0);
    CHECK(Run(Cmd_limit, bad_name, 3) == CMDLINE_INVALID_ARG);
}

static void test_event_command(void)
{
    char *get[] = {"event", "get"};

    reset_fixture();
    CHECK(Run(Cmd_event, get, 2) == CMDLINE_OK);
    CHECK(strcmp(console, "EVENT none\r\n") == 0);

    set_temperature(true, 900);
    AppLimits_Update();
    AppLimits_Update();
    CHECK(Run(Cmd_event, get, 2) == CMDLINE_OK);
    CHECK(strcmp(console, "EVENT seq=1 sev=WARNING code=TEMP mode=NORMAL value=900 time=unset source=UNSET\r\n") == 0);
}

static void test_limit_error_persists_until_retry(void)
{
    reset_fixture();
    set_temperature(true, 900);
    AppLimits_Update();
    AppLimits_Update();
    CHECK(nvm_limit_error && AppMode_Get() == APP_MODE_ERROR);
    CHECK(!AppLimits_Retry(true));
    CHECK(nvm_limit_error);

    set_temperature(true, 250);
    AppLimits_Update();
    CHECK(AppLimits_Retry(true));
    CHECK(!nvm_limit_error && AppMode_Get() == APP_MODE_NORMAL);

    set_temperature(true, 900);
    AppLimits_Update();
    AppLimits_Update();
    CHECK(nvm_limit_error);
    /* Reboot: RAM state is lost, the stored flag brings ERROR back. */
    AppMode_Retry(true, true);
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
    AppLimits_Init();
    CHECK(AppMode_Get() == APP_MODE_ERROR);
}

static void test_fault_clear_after_overcurrent(void)
{
    char *clear[] = {"fault", "clear"};
    reset_fixture();
    set_currents(APP_LIMITS_VOUT_DEFAULT_MA + 1U, 0U);
    AppLimits_Update();
    AppLimits_Update();
    AppLimits_Update();
    CHECK(nvm_limit_error && AppMode_LimitError());

    CHECK(Run(Cmd_fault, clear, 2) == CMDLINE_OK);
    CHECK(strcmp(console, "FAULT clear refused: limit still active\r\n") == 0);
    CHECK(nvm_limit_error && AppMode_LimitError());

    set_currents(0U, 0U);
    efuse_active = true;
    CHECK(Run(Cmd_fault, clear, 2) == CMDLINE_OK);
    CHECK(strcmp(console, "FAULT clear refused: fault still active\r\n") == 0);
    CHECK(nvm_limit_error && AppMode_LimitError());

    efuse_active = false;
    regulator_present = false;
    CHECK(Run(Cmd_fault, clear, 2) == CMDLINE_OK);
    CHECK(strcmp(console, "FAULT clear refused: regulator or limit unhealthy\r\n") == 0);
    CHECK(nvm_limit_error && AppMode_LimitError());

    regulator_present = true;
    CHECK(Run(Cmd_fault, clear, 2) == CMDLINE_OK);
    CHECK(strcmp(console, "FAULT cleared\r\n") == 0);
    CHECK(!nvm_limit_error && !AppMode_LimitError());
    CHECK(AppMode_Get() == APP_MODE_NORMAL);

    /* Clearing a recovered trip must not leave operator-controlled MANUAL mode. */
    CHECK(AppMode_RequestManual());
    AppMode_SetLimitError();
    BSP_NVM_WriteLimitError(true);
    CHECK(Run(Cmd_fault, clear, 2) == CMDLINE_OK);
    CHECK(!nvm_limit_error && !AppMode_LimitError());
    CHECK(AppMode_Get() == APP_MODE_MANUAL);
    CHECK(AppMode_RequestNormal());
}

static void test_fault_clear_all_rechecks_each_cause(void)
{
    char *clear_all[] = {"fault", "clear", "all"};
    uint32_t ms;
    reset_fixture();
    SCH_StartScheduler();
    AppMode_NoteRequest();
    for (ms = 0U; ms <= APP_MODE_COMMS_TIMEOUT_MS; ms++) SCH_RunSystemTickTimer();
    CHECK(AppMode_LinkLost());

    pac_present = false;
    eeprom_present = false;
    bus_fault = true;
    setpoint_mismatch = true;
    efuse_active = true;
    regulator_present = false;
    AppMode_SetRegulatorFault();
    AppMode_SetLimitError();
    BSP_NVM_WriteLimitError(true);
    set_currents(APP_LIMITS_VOUT_DEFAULT_MA + 1U, 0U);
    CHECK(Run(Cmd_fault, clear_all, 3) == CMDLINE_OK);
    CHECK(strcmp(console, "FAULT clear all incomplete; run mode get and fault get\r\n") == 0);
    CHECK(AppMode_RegulatorError() && AppMode_LimitError() && nvm_limit_error);
    CHECK(AppMode_PacUnreadable() && AppMode_EepromMissing());
    CHECK(AppMode_BusFault() && AppMode_SetpointMismatch());
    CHECK(!AppMode_LinkLost());

    pac_present = true;
    eeprom_present = true;
    bus_fault = false;
    setpoint_mismatch = false;
    efuse_active = false;
    regulator_present = true;
    set_currents(0U, 0U);
    CHECK(Run(Cmd_fault, clear_all, 3) == CMDLINE_OK);
    CHECK(strcmp(console, "FAULT clear all complete\r\n") == 0);
    CHECK(!AppMode_RegulatorError() && !AppMode_LimitError() && !nvm_limit_error);
    CHECK(!AppMode_PacUnreadable() && !AppMode_EepromMissing());
    CHECK(!AppMode_BusFault() && !AppMode_SetpointMismatch());
    CHECK(AppMode_Get() == APP_MODE_NORMAL);

    /* The acknowledgement restarts the timeout; it does not disable link monitoring. */
    for (ms = 0U; ms <= APP_MODE_COMMS_TIMEOUT_MS; ms++) SCH_RunSystemTickTimer();
    CHECK(AppMode_LinkLost());
}

int main(void)
{
    test_fault_clear_after_overcurrent();
    test_limit_error_persists_until_retry();
    test_limit_command();
    test_event_command();
    test_defaults_when_record_blank();
    test_loads_valid_record();
    test_set_validates_and_persists();
    test_temperature_trips_in_normal();
    test_trip_logged_once_per_excursion();
    test_trip_in_manual_logs_without_acting();
    test_current_trips_in_normal();
    test_trans_current_trips_in_normal();
    test_single_sample_and_invalid_do_not_trip();
    test_retry_refused_while_limit_active();
    test_active_reports_live_state();
    test_event_ring_wraps();
    test_fault_clear_all_rechecks_each_cause();
    if (failures != 0U)
    {
        fprintf(stderr, "%u app limits test failure(s)\n", failures);
        return 1;
    }
    printf("app limits tests passed\n");
    return 0;
}
