#include "command_led.h"
#include "command.h"
#include "cmdline.h"
#include "../SystemData/system_data.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static bool ParseDuration(const char *text, uint32_t *value)
{
    uint32_t parsed = 0U;
    if (text == 0 || *text == '\0' || value == 0) return false;
    while (*text != '\0')
    {
        uint32_t digit;
        if (*text < '0' || *text > '9') return false;
        digit = (uint32_t)(*text++ - '0');
        if (parsed > (UINT32_MAX - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
    }
    if (parsed == 0U) return false;
    *value = parsed;
    return true;
}

static void OutputUnsigned(CommandContext *context, uint32_t value)
{
    char digits[11];
    uint8_t pos = sizeof(digits) - 1U;
    digits[pos] = '\0';
    do
    {
        digits[--pos] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    Command_OutputString(context, &digits[pos]);
}

static void OutputTiming(CommandContext *context)
{
    SystemData_HeartbeatTiming timing = SystemData_GetHeartbeatTiming();
    COMMAND_OUT(context, "LED on=");
    OutputUnsigned(context, timing.on_ms);
    COMMAND_OUT(context, " ms off=");
    OutputUnsigned(context, timing.off_ms);
    COMMAND_OUT(context, " ms\r\n");
}

int Cmd_led(CommandContext *context, int argc, char *argv[])
{
    uint32_t on_ms;
    uint32_t off_ms;
    if (argc < 2) return CMDLINE_TOO_FEW_ARGS;
    if (strcmp(argv[1], "get") == 0)
    {
        if (argc != 2) return CMDLINE_TOO_MANY_ARGS;
        OutputTiming(context);
        return CMDLINE_OK;
    }
    if (strcmp(argv[1], "set") != 0) return CMDLINE_INVALID_ARG;
    if (argc < 4) return CMDLINE_TOO_FEW_ARGS;
    if (argc > 4) return CMDLINE_TOO_MANY_ARGS;
    if (!ParseDuration(argv[2], &on_ms) || !ParseDuration(argv[3], &off_ms) ||
        !SystemData_SetHeartbeatTiming(on_ms, off_ms)) return CMDLINE_INVALID_ARG;
    OutputTiming(context);
    return CMDLINE_OK;
}
