#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_config.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_event.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_limits.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_mode.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Scheduler/scheduler.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NVM/bsp_nvm.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/cmdline.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_config.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_event.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_limit.h"

static unsigned failures;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

/* Console capture for the config commands. */
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

static int RunArgs(int (*command)(CommandContext *, int, char **), int argc, char **args)
{
    CommandContext context;
    memset(&context, 0, sizeof(context));
    console_len = 0U;
    console[0] = '\0';
    return command(&context, argc, args);
}

#define RUN(command, ...) RunArgs((command), \
    (int)(sizeof((char *[]){__VA_ARGS__}) / sizeof(char *)), (char *[]){__VA_ARGS__})

/* 24C04 stand-in, for the event log. */
static uint8_t eeprom[512];

/* Internal EEPROM stand-ins for the config and limit records. */
static BSP_NVM_Config nvm_config;
static bool nvm_config_valid;
static unsigned nvm_config_writes;
static BSP_NVM_Limits nvm_limits;
static bool nvm_limits_valid;
static bool nvm_limit_error;

bool BSP_NVM_ReadConfig(BSP_NVM_Config *config)
{
    if (!nvm_config_valid) return false;
    *config = nvm_config;
    return true;
}

void BSP_NVM_WriteConfig(const BSP_NVM_Config *config)
{
    nvm_config = *config;
    nvm_config_valid = true;
    nvm_config_writes++;
}

bool BSP_NVM_ReadLimits(BSP_NVM_Limits *limits)
{
    if (!nvm_limits_valid) return false;
    *limits = nvm_limits;
    return true;
}

void BSP_NVM_WriteLimits(const BSP_NVM_Limits *limits)
{
    nvm_limits = *limits;
    nvm_limits_valid = true;
}

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
}

void AppRetry_NoteShutdown(void)
{
}

static void advance_ms(uint32_t ms)
{
    while (ms-- != 0U) SCH_RunSystemTickTimer();
}

static void reset_fixture(void)
{
    SystemData_Init();
    SCH_Initialize();
    SCH_StartScheduler();
    memset(eeprom, 0xFF, sizeof(eeprom));
    nvm_config_valid = false;
    nvm_config_writes = 0U;
    nvm_limits_valid = false;
    nvm_limit_error = false;
    AppConfig_Init();
    AppLimits_Init();
    AppMode_SelfTest(true, true, true);
    AppMode_SetLinkTimeoutMs(APP_CONFIG_COMMS_TIMEOUT_DEFAULT_S * 1000UL);
}

static bool LastEventIsConfigChange(void)
{
    AppEvent event;
    if (!AppEvent_Read(0U, &event)) return false;
    return event.severity == APP_EVENT_SEVERITY_INFO && event.code == APP_EVENT_CONFIG_CHANGED;
}

static void test_defaults_when_blank(void)
{
    BSP_NVM_Config config;
    reset_fixture();
    AppConfig_Get(&config);
    CHECK(config.slave_address == 1U && config.baud_code == 6U && config.comms_timeout_s == 5U);
    CHECK(config.retry_enable == 1U && config.retry_time_s == 5U);
    CHECK(AppConfig_DefaultsInUse());
    CHECK(RUN(Cmd_config, "config") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG address=1 baud=115200 timeout=5 retry_enable=1 retry_time=5 source=DEFAULT\r\n") == 0);
}

static void test_damaged_or_invalid_record_falls_back(void)
{
    BSP_NVM_Config config;
    reset_fixture();
    nvm_config_valid = true;
    nvm_config.slave_address = 0U; /* outside 1..247 */
    nvm_config.baud_code = 3U;
    nvm_config.comms_timeout_s = 5U;
    nvm_config.retry_enable = 1U;
    nvm_config.retry_time_s = 5U;
    AppConfig_Init();
    AppConfig_Get(&config);
    CHECK(config.slave_address == 1U);
    CHECK(AppConfig_DefaultsInUse());

    nvm_config.slave_address = 7U;
    nvm_config.baud_code = 9U; /* unknown code */
    AppConfig_Init();
    AppConfig_Get(&config);
    CHECK(config.slave_address == 1U && AppConfig_DefaultsInUse());
}

static void test_set_address_persists_and_logs(void)
{
    BSP_NVM_Config config;
    reset_fixture();
    CHECK(RUN(Cmd_config, "config", "set", "address", "7") == CMDLINE_OK);
    CHECK(strcmp(console,
                 "CONFIG address=7 baud=115200 timeout=5 retry_enable=1 retry_time=5 source=EEPROM\r\n"
                 "Address and baud take effect after reboot\r\n") == 0);
    AppConfig_Get(&config);
    CHECK(config.slave_address == 7U && !AppConfig_DefaultsInUse());
    CHECK(nvm_config.slave_address == 7U && nvm_config_writes == 1U);
    CHECK(LastEventIsConfigChange());
    CHECK(RUN(Cmd_event, "event", "get") == CMDLINE_OK);
    CHECK(strstr(console, "EVENT seq=1 sev=INFO code=CONFIG_CHANGED") != 0);
}

static void test_set_baud_and_timeout(void)
{
    BSP_NVM_Config config;
    reset_fixture();
    CHECK(RUN(Cmd_config, "config", "set", "baud", "19200") == CMDLINE_OK);
    CHECK(strcmp(console,
                 "CONFIG address=1 baud=19200 timeout=5 retry_enable=1 retry_time=5 source=EEPROM\r\n"
                 "Address and baud take effect after reboot\r\n") == 0);
    AppConfig_Get(&config);
    CHECK(config.baud_code == 4U);

    /* The timeout is applied at once and needs no reboot. */
    CHECK(RUN(Cmd_config, "config", "set", "timeout", "10") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG address=1 baud=19200 timeout=10 retry_enable=1 retry_time=5 source=EEPROM\r\n") == 0);
    AppMode_NoteRequest();
    advance_ms(10000U);
    CHECK(!AppMode_LinkLost());
    advance_ms(1U);
    CHECK(AppMode_LinkLost());
}

static void test_115200_baud_persists(void)
{
    BSP_NVM_Config config;
    reset_fixture();
    CHECK(RUN(Cmd_config, "config", "set", "baud", "115200") == CMDLINE_OK);
    CHECK(strstr(console, "baud=115200") != 0);
    AppConfig_Init(); /* Simulated restart. */
    AppConfig_Get(&config);
    CHECK(config.baud_code == 6U);
    CHECK(AppConfig_BaudRate(config.baud_code) == 115200UL);
}

static void test_retry_settings_persist(void)
{
    BSP_NVM_Config config;
    reset_fixture();
    CHECK(RUN(Cmd_config, "config", "set", "retry_enable", "0") == CMDLINE_OK);
    CHECK(strstr(console, "retry_enable=0 retry_time=5 source=EEPROM") != 0);
    CHECK(RUN(Cmd_config, "config", "set", "retry_time", "12") == CMDLINE_OK);
    CHECK(strstr(console, "retry_enable=0 retry_time=12 source=EEPROM") != 0);
    AppConfig_Init(); /* Simulated restart: load the stored values. */
    AppConfig_Get(&config);
    CHECK(config.retry_enable == 0U && config.retry_time_s == 12U);
    CHECK(RUN(Cmd_config, "config", "set", "retry_enable", "2") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: out of range\r\n") == 0);
    CHECK(RUN(Cmd_config, "config", "set", "retry_time", "0") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: out of range\r\n") == 0);
    CHECK(RUN(Cmd_config, "config", "set", "retry_time", "61") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: out of range\r\n") == 0);
}

static void test_refused_values_change_nothing(void)
{
    BSP_NVM_Config config;
    AppEvent event;
    reset_fixture();
    CHECK(RUN(Cmd_config, "config", "set", "address", "0") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: out of range\r\n") == 0);
    CHECK(RUN(Cmd_config, "config", "set", "address", "248") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: out of range\r\n") == 0);
    CHECK(RUN(Cmd_config, "config", "set", "baud", "115201") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: unsupported baud\r\n") == 0);
    CHECK(RUN(Cmd_config, "config", "set", "baud", "9601") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: unsupported baud\r\n") == 0);
    CHECK(RUN(Cmd_config, "config", "set", "timeout", "0") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: out of range\r\n") == 0);
    CHECK(RUN(Cmd_config, "config", "set", "timeout", "61") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: out of range\r\n") == 0);
    CHECK(RUN(Cmd_config, "config", "set", "address", "99999999999") == CMDLINE_OK);
    CHECK(strcmp(console, "CONFIG refused: out of range\r\n") == 0);

    AppConfig_Get(&config);
    CHECK(config.slave_address == 1U && config.baud_code == 6U && config.comms_timeout_s == 5U);
    CHECK(config.retry_enable == 1U && config.retry_time_s == 5U);
    CHECK(nvm_config_writes == 0U && !AppEvent_Read(0U, &event));
    CHECK(RUN(Cmd_config, "config", "set", "parity", "1") == CMDLINE_INVALID_ARG);
    CHECK(RUN(Cmd_config, "config", "set", "address") == CMDLINE_INVALID_ARG);
    CHECK(RUN(Cmd_config, "config", "set", "address", "x7") == CMDLINE_INVALID_ARG);
}

static void test_config_reset_needs_confirm(void)
{
    reset_fixture();
    CHECK(RUN(Cmd_config, "config", "set", "address", "7") == CMDLINE_OK);
    CHECK(RUN(Cmd_config_reset, "config_reset") == CMDLINE_INVALID_ARG);
    CHECK(RUN(Cmd_config_reset, "config_reset", "yes") == CMDLINE_INVALID_ARG);
    CHECK(nvm_config.slave_address == 7U);
}

static void test_config_reset_restores_defaults(void)
{
    BSP_NVM_Config config;
    BSP_NVM_Limits limits;
    reset_fixture();
    CHECK(RUN(Cmd_config, "config", "set", "address", "7") == CMDLINE_OK);
    CHECK(RUN(Cmd_config, "config", "set", "baud", "19200") == CMDLINE_OK);
    CHECK(RUN(Cmd_config, "config", "set", "timeout", "10") == CMDLINE_OK);
    CHECK(RUN(Cmd_config, "config", "set", "retry_enable", "0") == CMDLINE_OK);
    CHECK(RUN(Cmd_config, "config", "set", "retry_time", "12") == CMDLINE_OK);
    limits.temp_trip_tenths_c = 600;
    limits.vout_max_ma = 1000U;
    limits.trans5v_max_ma = 1500U;
    CHECK(AppLimits_Set(&limits));

    CHECK(RUN(Cmd_config_reset, "config_reset", "confirm") == CMDLINE_OK);
    CHECK(strcmp(console,
                 "CONFIG reset to defaults\r\n"
                 "CONFIG address=1 baud=115200 timeout=5 retry_enable=1 retry_time=5 source=EEPROM\r\n"
                 "LIMIT temp=850 vout=1000 trans=1000\r\n"
                 "Address and baud take effect after reboot\r\n") == 0);
    AppConfig_Get(&config);
    CHECK(config.slave_address == 1U && config.baud_code == 6U && config.comms_timeout_s == 5U);
    CHECK(config.retry_enable == 1U && config.retry_time_s == 5U);
    AppLimits_Get(&limits);
    CHECK(limits.temp_trip_tenths_c == 850 && limits.vout_max_ma == 1000U && limits.trans5v_max_ma == 1000U);
    CHECK(nvm_limits.vout_max_ma == 1000U);
    CHECK(LastEventIsConfigChange());

    /* The timeout is back at 5 s. */
    AppMode_NoteRequest();
    advance_ms(5000U);
    CHECK(!AppMode_LinkLost());
    advance_ms(1U);
    CHECK(AppMode_LinkLost());

    /* Nothing to reboot for when address and baud are already at their defaults. */
    CHECK(RUN(Cmd_config_reset, "config_reset", "confirm") == CMDLINE_OK);
    CHECK(strstr(console, "reboot") == 0);
}

int main(void)
{
    test_defaults_when_blank();
    test_damaged_or_invalid_record_falls_back();
    test_set_address_persists_and_logs();
    test_set_baud_and_timeout();
    test_115200_baud_persists();
    test_retry_settings_persist();
    test_refused_values_change_nothing();
    test_config_reset_needs_confirm();
    test_config_reset_restores_defaults();

    if (failures != 0U)
    {
        fprintf(stderr, "%u config test(s) failed\n", failures);
        return 1;
    }
    puts("app config tests passed");
    return 0;
}
