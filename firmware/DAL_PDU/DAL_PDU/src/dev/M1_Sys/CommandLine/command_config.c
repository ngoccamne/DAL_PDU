#include "command_config.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "command_limit.h"
#include "../../M0_App/app_config.h"
#include "../../M0_App/app_event.h"
#include "../../M0_App/app_limits.h"
#include "../../M0_App/app_mode.h"

/* Values above this are saturated, so they are refused rather than wrapped. */
#define PARSE_SATURATE 1000000UL

/* Parses decimal digits only. Returns false on any other text. */
static bool ParseUnsigned(const char *text, uint32_t *result)
{
    uint32_t value = 0U;
    if (text == 0 || *text == '\0') return false;
    while (*text != '\0')
    {
        if (*text < '0' || *text > '9') return false;
        value = value * 10U + (uint32_t)(*text - '0');
        if (value > PARSE_SATURATE) value = PARSE_SATURATE + 1U;
        text++;
    }
    *result = value;
    return true;
}

static void OutputUnsigned(CommandContext *context, uint32_t value)
{
    char digits[10];
    uint8_t count = 0U;
    do
    {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U && count < sizeof(digits));
    while (count > 0U)
    {
        char out[2] = {digits[--count], '\0'};
        Command_OutputString(context, out);
    }
}

static void OutputConfig(CommandContext *context)
{
    BSP_NVM_Config config;
    AppConfig_Get(&config);
    COMMAND_OUT(context, "CONFIG address=");
    OutputUnsigned(context, config.slave_address);
    COMMAND_OUT(context, " baud=");
    OutputUnsigned(context, AppConfig_BaudRate(config.baud_code));
    COMMAND_OUT(context, " timeout=");
    OutputUnsigned(context, config.comms_timeout_s);
    COMMAND_OUT(context, " retry_enable=");
    OutputUnsigned(context, config.retry_enable);
    COMMAND_OUT(context, " retry_time=");
    OutputUnsigned(context, config.retry_time_s);
    if (AppConfig_DefaultsInUse()) COMMAND_OUT(context, " source=DEFAULT\r\n");
    else COMMAND_OUT(context, " source=EEPROM\r\n");
}

/* config get | config set address <1-247> | config set baud <bps> | config set timeout <s> */
int Cmd_config(CommandContext *context, int argc, char *argv[])
{
    BSP_NVM_Config config;
    uint32_t value;
    bool needs_reboot;

    if (argc > 4) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "get") == 0))
    {
        OutputConfig(context);
        return CMDLINE_OK;
    }
    if (argc != 4 || strcmp(argv[1], "set") != 0) return CMDLINE_INVALID_ARG;
    if (!ParseUnsigned(argv[3], &value)) return CMDLINE_INVALID_ARG;

    AppConfig_Get(&config);
    if (strcmp(argv[2], "address") == 0)
    {
        if (value < APP_CONFIG_SLAVE_ADDRESS_MIN || value > APP_CONFIG_SLAVE_ADDRESS_MAX)
        {
            COMMAND_OUT(context, "CONFIG refused: out of range\r\n");
            return CMDLINE_OK;
        }
        config.slave_address = (uint8_t)value;
        needs_reboot = true;
    }
    else if (strcmp(argv[2], "baud") == 0)
    {
        if (!AppConfig_BaudCodeFor(value, &config.baud_code))
        {
            COMMAND_OUT(context, "CONFIG refused: unsupported baud\r\n");
            return CMDLINE_OK;
        }
        needs_reboot = true;
    }
    else if (strcmp(argv[2], "timeout") == 0)
    {
        if (value < APP_CONFIG_COMMS_TIMEOUT_MIN_S || value > APP_CONFIG_COMMS_TIMEOUT_MAX_S)
        {
            COMMAND_OUT(context, "CONFIG refused: out of range\r\n");
            return CMDLINE_OK;
        }
        config.comms_timeout_s = (uint8_t)value;
        needs_reboot = false;
    }
    else if (strcmp(argv[2], "retry_enable") == 0)
    {
        if (value > 1U)
        {
            COMMAND_OUT(context, "CONFIG refused: out of range\r\n");
            return CMDLINE_OK;
        }
        config.retry_enable = (uint8_t)value;
        needs_reboot = false;
    }
    else if (strcmp(argv[2], "retry_time") == 0)
    {
        if (value < APP_CONFIG_RETRY_TIME_MIN_S || value > APP_CONFIG_RETRY_TIME_MAX_S)
        {
            COMMAND_OUT(context, "CONFIG refused: out of range\r\n");
            return CMDLINE_OK;
        }
        config.retry_time_s = (uint8_t)value;
        needs_reboot = false;
    }
    else
    {
        return CMDLINE_INVALID_ARG;
    }

    if (!AppConfig_Set(&config))
    {
        COMMAND_OUT(context, "CONFIG refused: out of range\r\n");
        return CMDLINE_OK;
    }
    /* The timeout takes effect now. The address and baud wait for the next boot. */
    AppMode_SetLinkTimeoutMs((uint32_t)config.comms_timeout_s * 1000UL);
    AppEvent_Log(APP_EVENT_SEVERITY_INFO, APP_EVENT_CONFIG_CHANGED, 0);
    OutputConfig(context);
    if (needs_reboot) COMMAND_OUT(context, "Address and baud take effect after reboot\r\n");
    return CMDLINE_OK;
}

/* config_reset confirm: restores the configuration and the limits to the compiled defaults. */
int Cmd_config_reset(CommandContext *context, int argc, char *argv[])
{
    BSP_NVM_Config before;
    BSP_NVM_Config defaults;
    BSP_NVM_Limits limits;
    char limit_name[] = "limit";
    char *limit_argv[] = {limit_name};

    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
    if (argc != 2 || strcmp(argv[1], "confirm") != 0) return CMDLINE_INVALID_ARG;

    AppConfig_Get(&before);
    AppConfig_Reset();
    AppLimits_Defaults(&limits);
    if (!AppLimits_Set(&limits))
    {
        COMMAND_OUT(context, "CONFIG_RESET failed: limits not stored\r\n");
        return CMDLINE_OK;
    }
    AppConfig_Get(&defaults);
    AppMode_SetLinkTimeoutMs((uint32_t)defaults.comms_timeout_s * 1000UL);
    AppEvent_Log(APP_EVENT_SEVERITY_INFO, APP_EVENT_CONFIG_CHANGED, 0);

    COMMAND_OUT(context, "CONFIG reset to defaults\r\n");
    OutputConfig(context);
    (void)Cmd_limit(context, 1, limit_argv);
    if (before.slave_address != defaults.slave_address || before.baud_code != defaults.baud_code)
    {
        COMMAND_OUT(context, "Address and baud take effect after reboot\r\n");
    }
    return CMDLINE_OK;
}
