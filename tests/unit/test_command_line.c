#include <stdio.h>
#include <string.h>

#include "cmdline.h"
#include "command.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Version/version.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_power.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/I2C/bsp_i2c.h"

static unsigned int failures;
static char output[2048];
static size_t output_length;
static int output_overflow;
static int probe_calls;
static int probe_argc;
static char *probe_argv[3];
static bool trans_on;
static uint8_t i2c_address;
static uint8_t i2c_register;
static uint8_t i2c_value;
static void capture_output(void *user, const char *text);
static CommandContext test_context = {capture_output, &output, false, 0U};

/* TPS55287 command behavior is covered by test_tps55287.c. */
int Cmd_tps55287(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}

int Cmd_setpoint(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}


int Cmd_pac1934(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}

/* The mode command is covered by test_app_mode.c. */
int Cmd_mode(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}

/* The fault latch command is covered by test_fault_latch.c. */
int Cmd_fault(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}

int Cmd_limit(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}

int Cmd_config(CommandContext *context, int argc, char *argv[])
{
    (void)context; (void)argc; (void)argv;
    return CMDLINE_OK;
}

int Cmd_config_reset(CommandContext *context, int argc, char *argv[])
{
    (void)context; (void)argc; (void)argv;
    return CMDLINE_OK;
}

int Cmd_time(CommandContext *context, int argc, char *argv[])
{
    (void)context; (void)argc; (void)argv;
    return CMDLINE_OK;
}

int Cmd_status(CommandContext *context, int argc, char *argv[])
{
    (void)context; (void)argc; (void)argv;
    return CMDLINE_OK;
}

int Cmd_reset(CommandContext *context, int argc, char *argv[])
{
    (void)context; (void)argc; (void)argv;
    return CMDLINE_OK;
}

int Cmd_event(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}


/* The temperature command is covered by test_temperature.c. */
int Cmd_temp(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}

int Cmd_telemetry(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}

int Cmd_bootinfo(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}



/* The EEPROM command has its own focused parser and BSP test. */
int Cmd_eeprom(CommandContext *context, int argc, char *argv[])
{
    (void)context;
    (void)argc;
    (void)argv;
    return CMDLINE_OK;
}

bool AppPower_SetTrans(bool enable)
{
    trans_on = enable;
    SystemData_PowerStatus status = {enable, false, false, false, false};
    SystemData_SetPowerStatus(status);
    return true;
}

void AppPower_TurnOffOutputs(void)
{
}

void BSP_NVM_EraseAll(void)
{
}

bool BSP_EEPROM_EraseAll(void)
{
    return true;
}

void BSP_Reset_Software(void)
{
}

bool BSP_I2C_Probe(uint8_t address) { i2c_address = address; return address == 0x50U; }
bool BSP_I2C_ReadByte(uint8_t address, uint8_t *value)
{
    i2c_address = address;
    *value = 0xA5U;
    return true;
}
bool BSP_I2C_ReadRegister(uint8_t address, uint8_t reg, uint8_t *value)
{
    i2c_address = address;
    i2c_register = reg;
    *value = 0xA5U;
    return true;
}
bool BSP_I2C_WriteRegister(uint8_t address, uint8_t reg, uint8_t value)
{
    i2c_address = address;
    i2c_register = reg;
    i2c_value = value;
    return true;
}

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expression); \
        failures++; \
    } \
} while (0)

static void capture_output(void *user, const char *text)
{
    size_t length = strlen(text);

    if (user != &output)
    {
        failures++;
        return;
    }

    if (length >= sizeof(output) - output_length)
    {
        output_overflow = 1;
        return;
    }

    memcpy(output + output_length, text, length + 1U);
    output_length += length;
}

static void reset_output(void)
{
    output[0] = '\0';
    output_length = 0U;
    output_overflow = 0;
}

static int probe_handler(CommandContext *context, int argc, char *argv[])
{
    CHECK(context == &test_context);
    probe_calls++;
    probe_argc = argc;
    if (argc == 3)
    {
        probe_argv[0] = argv[0];
        probe_argv[1] = argv[1];
        probe_argv[2] = argv[2];
    }
    return CMDLINE_INVALID_ARG;
}

static void test_parser_passes_arguments_and_handler_result(void)
{
    tCmdLineEntry original = g_psCmdTable[0];
    char line[] = "  probe   alpha  beta  ";

    probe_calls = 0;
    probe_argc = 0;
    g_psCmdTable[0].pcCmd = "probe";
    g_psCmdTable[0].pfnCmd = probe_handler;

    reset_output();
    CHECK(CmdLineProcess(&test_context, line) == CMDLINE_INVALID_ARG);
    CHECK(probe_calls == 1);
    CHECK(probe_argc == 3);
    if (probe_argc == 3)
    {
        CHECK(strcmp(probe_argv[0], "probe") == 0);
        CHECK(strcmp(probe_argv[1], "alpha") == 0);
        CHECK(strcmp(probe_argv[2], "beta") == 0);
    }
    CHECK(output_length == 0U);

    g_psCmdTable[0] = original;
}

static void test_version_and_argument_errors(void)
{
    char version[] = "version";
    char padded[] = "  version   ";
    char extra[] = "version  extra";

    reset_output();
    CHECK(CmdLineProcess(&test_context, version) == CMDLINE_OK);
    CHECK(strcmp(output, "DAL_PDU firmware v" FIRMWARE_VERSION
                         " (" FIRMWARE_STATUS ")" " build " FIRMWARE_BUILD_ID "\r\n") == 0);
    CHECK(output_overflow == 0);

    reset_output();
    CHECK(CmdLineProcess(&test_context, padded) == CMDLINE_OK);
    CHECK(strcmp(output, "DAL_PDU firmware v" FIRMWARE_VERSION
                         " (" FIRMWARE_STATUS ")" " build " FIRMWARE_BUILD_ID "\r\n") == 0);

    reset_output();
    CHECK(CmdLineProcess(&test_context, extra) == CMDLINE_TOO_MANY_ARGS);
    CHECK(output_length == 0U);
}

static void test_help_lists_registered_commands(void)
{
    char help[] = "help";

    reset_output();
    CHECK(CmdLineProcess(&test_context, help) == CMDLINE_OK);
    while (test_context.help_pending) Command_ContinueOutput(&test_context);
    CHECK(strcmp(output,
                 "\r\nAvailable commands\r\n"
                 "------------------\r\n"
                 "help : Display list of commands, format: help\r\n"
                 "version : Display firmware version, format: version\r\n"
                 "led : LED timing, format: led get | led set <on_ms> <off_ms>\r\n"
                 "trans : TRANS power, format: trans on | trans off | trans get\r\n"
                 "i2cdetect : Scan I2C bus, format: i2cdetect [first last]\r\n"
                 "i2cget : Read I2C byte, format: i2cget <addr> [reg]\r\n"
                 "i2cset : Write I2C byte, format: i2cset <addr> <reg> <value>\r\n"
                 "tps55287 : U5 VOUT, format: tps55287 status | get | set <mV> | on | off\r\n"
                 "setpoint : Commanded VOUT setpoint from system data, format: setpoint get\r\n"
                 "pac1934 : Rail monitor, format: pac1934 [get | status | refresh]\r\n"
                 "fault : Fault status, format: fault [get | clear | clear all]\r\n"
                 "limit : Limits, format: limit [get | temp <tenths C> | vout <mA> | trans <mA>]\r\n"
                 "config : Config, format: config [get | set address <1-247> | set baud <bps> | set timeout <1-60 s> | set retry_enable <0|1> | set retry_time <1-60 s>]\r\n"
                 "config_reset : Reset config and limits to defaults, format: config_reset confirm\r\n"
                 "event : Event log, format: event [get]\r\n"
                 "mode : Operating mode, format: mode [get | normal | manual | retry]\r\n"
                 "temp : Board temperature, format: temp [get | refresh]\r\n"
                 "telemetry : Setpoint and rail telemetry, format: telemetry\r\n"
                 "bootinfo : Boot count, last cause, and history, format: bootinfo [get | history]\r\n"
                 "time : Clock, format: time [get | set <unix seconds>]\r\n"
                 "status : Status summary, format: status\r\n"
                 "reset : Restart the MCU with outputs off, format: reset confirm\r\n"
                 "eeprom : U14 24C04, format: eeprom probe | read <addr> [len] | write <addr> <hexbytes>\r\n"
                 "persist_reset : Erase EEPROM to defaults, format: persist_reset internal confirm | persist_reset external confirm\r\n") == 0);
    CHECK(output_overflow == 0);
}

static void test_led_timing_commands(void)
{
    char get_default[] = "led get";
    char set[] = "led set 250 750";
    char get_updated[] = "led get";
    char zero[] = "led set 0 100";
    char negative[] = "led set -1 100";
    char overflow[] = "led set 4294967296 100";
    char missing[] = "led set 100";
    char extra[] = "led get extra";
    SystemData_HeartbeatTiming timing;

    SystemData_Init();
    reset_output();
    CHECK(CmdLineProcess(&test_context, get_default) == CMDLINE_OK);
    CHECK(strcmp(output, "LED on=1000 ms off=1000 ms\r\n") == 0);
    reset_output();
    CHECK(CmdLineProcess(&test_context, set) == CMDLINE_OK);
    CHECK(strcmp(output, "LED on=250 ms off=750 ms\r\n") == 0);
    reset_output();
    CHECK(CmdLineProcess(&test_context, get_updated) == CMDLINE_OK);
    CHECK(strcmp(output, "LED on=250 ms off=750 ms\r\n") == 0);
    CHECK(CmdLineProcess(&test_context, zero) == CMDLINE_INVALID_ARG);
    CHECK(CmdLineProcess(&test_context, negative) == CMDLINE_INVALID_ARG);
    CHECK(CmdLineProcess(&test_context, overflow) == CMDLINE_INVALID_ARG);
    CHECK(CmdLineProcess(&test_context, missing) == CMDLINE_TOO_FEW_ARGS);
    CHECK(CmdLineProcess(&test_context, extra) == CMDLINE_TOO_MANY_ARGS);
    timing = SystemData_GetHeartbeatTiming();
    CHECK(timing.on_ms == 250U && timing.off_ms == 750U);
    SystemData_Init();
    timing = SystemData_GetHeartbeatTiming();
    CHECK(timing.on_ms == 1000U && timing.off_ms == 1000U);
}

static void test_unknown_empty_and_case_sensitive_commands(void)
{
    char empty[] = "";
    char spaces[] = "    ";
    char unknown[] = "stat";
    char wrong_case[] = "Version";

    reset_output();
    CHECK(CmdLineProcess(&test_context, empty) == CMDLINE_BAD_CMD);
    CHECK(CmdLineProcess(&test_context, spaces) == CMDLINE_BAD_CMD);
    CHECK(CmdLineProcess(&test_context, unknown) == CMDLINE_BAD_CMD);
    CHECK(CmdLineProcess(&test_context, wrong_case) == CMDLINE_BAD_CMD);
    CHECK(output_length == 0U);
}

static void test_trans_and_i2c_commands(void)
{
    char trans_on_command[] = "trans on";
    char trans_off_command[] = "trans off";
    char detect[] = "i2cdetect 0x50 0x50";
    char get[] = "i2cget 0x50 0x02";
    char set[] = "i2cset 80 2 255";
    char invalid[] = "i2cget 0x80";

    reset_output();
    CHECK(CmdLineProcess(&test_context, trans_on_command) == CMDLINE_OK);
    CHECK(trans_on && strstr(output, "TRANS on") != NULL);
    reset_output();
    CHECK(CmdLineProcess(&test_context, trans_off_command) == CMDLINE_OK);
    CHECK(!trans_on && strstr(output, "TRANS off") != NULL);
    reset_output();
    CHECK(CmdLineProcess(&test_context, detect) == CMDLINE_OK);
    CHECK(i2c_address == 0x50U && strstr(output, "0x50") != NULL);
    reset_output();
    CHECK(CmdLineProcess(&test_context, get) == CMDLINE_OK);
    CHECK(i2c_address == 0x50U && i2c_register == 2U);
    CHECK(strcmp(output, "0xa5\r\n") == 0);
    reset_output();
    CHECK(CmdLineProcess(&test_context, set) == CMDLINE_OK);
    CHECK(i2c_address == 0x50U && i2c_register == 2U && i2c_value == 255U);
    CHECK(CmdLineProcess(&test_context, invalid) == CMDLINE_INVALID_ARG);
}

static void test_argument_limit_and_repeated_calls(void)
{
    char eight[] = "help a b c d e f g";
    char nine[] = "help a b c d e f g h";
    char version[] = "version";

    reset_output();
    CHECK(CmdLineProcess(&test_context, eight) == CMDLINE_OK);
    CHECK(strstr(output, "Available commands") != NULL);

    reset_output();
    CHECK(CmdLineProcess(&test_context, nine) == CMDLINE_TOO_MANY_ARGS);
    CHECK(output_length == 0U);

    reset_output();
    CHECK(CmdLineProcess(&test_context, version) == CMDLINE_OK);
    CHECK(strstr(output, "DAL_PDU firmware v") == output);
}

int main(void)
{
    test_parser_passes_arguments_and_handler_result();
    test_version_and_argument_errors();
    test_help_lists_registered_commands();
    test_led_timing_commands();
    test_trans_and_i2c_commands();
    test_unknown_empty_and_case_sensitive_commands();
    test_argument_limit_and_repeated_calls();

    if (failures != 0U)
    {
        fprintf(stderr, "%u command line checks failed\n", failures);
        return 1;
    }

    puts("command line tests passed");
    return 0;
}
