#include "command_time.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "../../M0_App/app_clock.h"
#include "../../M0_App/app_event.h"

/* Parses decimal digits into a 32-bit value. Returns false on any other text or on overflow. */
static bool ParseUnsigned(const char *text, uint32_t *result)
{
    uint32_t value = 0U;
    uint32_t digit;
    if (text == 0 || *text == '\0') return false;
    for (; *text != '\0'; text++)
    {
        if (*text < '0' || *text > '9') return false;
        digit = (uint32_t)(*text - '0');
        if (value > (0xFFFFFFFFUL - digit) / 10U) return false;
        value = value * 10U + digit;
    }
    *result = value;
    return true;
}

/* Writes value with exactly width digits, zero padded. */
static void OutputDigits(CommandContext *context, uint32_t value, uint8_t width)
{
    uint32_t divisor = 1U;
    uint8_t i;
    char out[2] = {0, '\0'};
    for (i = 1U; i < width; i++) divisor *= 10U;
    for (i = 0U; i < width; i++)
    {
        out[0] = (char)('0' + (value / divisor) % 10U);
        Command_OutputString(context, out);
        divisor /= 10U;
    }
}

void Command_OutputClock(CommandContext *context, uint32_t unix_s)
{
    AppClock_Calendar calendar;
    if (unix_s == APP_CLOCK_UNSET)
    {
        COMMAND_OUT(context, "unset");
        return;
    }
    AppClock_ToCalendar(unix_s, &calendar);
    OutputDigits(context, calendar.year, 4U);
    COMMAND_OUT(context, "-");
    OutputDigits(context, calendar.month, 2U);
    COMMAND_OUT(context, "-");
    OutputDigits(context, calendar.day, 2U);
    COMMAND_OUT(context, "T");
    OutputDigits(context, calendar.hour, 2U);
    COMMAND_OUT(context, ":");
    OutputDigits(context, calendar.minute, 2U);
    COMMAND_OUT(context, ":");
    OutputDigits(context, calendar.second, 2U);
    COMMAND_OUT(context, "Z");
}

void Command_OutputClockSource(CommandContext *context, uint8_t source)
{
    switch (source)
    {
    case APP_CLOCK_SOURCE_SET: COMMAND_OUT(context, "SET"); break;
    case APP_CLOCK_SOURCE_RESTORED: COMMAND_OUT(context, "RESTORED"); break;
    default: COMMAND_OUT(context, "UNSET"); break;
    }
}

static void OutputTime(CommandContext *context)
{
    COMMAND_OUT(context, "TIME ");
    Command_OutputClock(context, AppClock_Now());
    COMMAND_OUT(context, " source=");
    Command_OutputClockSource(context, AppClock_Source());
    COMMAND_OUT(context, "\r\n");
}

/* time [get] | time set <unix seconds>. The set is logged as a CLOCK_SET event. */
int Cmd_time(CommandContext *context, int argc, char *argv[])
{
    uint32_t unix_s;
    if (argc > 3) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "get") == 0))
    {
        OutputTime(context);
        return CMDLINE_OK;
    }
    if (argc != 3 || strcmp(argv[1], "set") != 0) return CMDLINE_INVALID_ARG;
    if (!ParseUnsigned(argv[2], &unix_s)) return CMDLINE_INVALID_ARG;
    if (!AppClock_Set(unix_s))
    {
        COMMAND_OUT(context, "TIME refused: reserved value\r\n");
        return CMDLINE_OK;
    }
    AppEvent_Log(APP_EVENT_SEVERITY_INFO, APP_EVENT_CLOCK_SET, 0);
    OutputTime(context);
    return CMDLINE_OK;
}
