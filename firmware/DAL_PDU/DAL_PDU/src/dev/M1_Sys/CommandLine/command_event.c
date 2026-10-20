#include "command_event.h"

#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "../../M0_App/app_event.h"
#include "command_time.h"

static void OutputCode(CommandContext *context, uint8_t code)
{
    switch (code)
    {
    case APP_EVENT_TEMP_LIMIT: COMMAND_OUT(context, "TEMP"); break;
    case APP_EVENT_VOUT_CURRENT: COMMAND_OUT(context, "VOUT_CURRENT"); break;
    case APP_EVENT_TRANS_CURRENT: COMMAND_OUT(context, "TRANS_CURRENT"); break;
    case APP_EVENT_I2C_TIMEOUT: COMMAND_OUT(context, "I2C_TIMEOUT"); break;
    case APP_EVENT_CLOCK_SET: COMMAND_OUT(context, "CLOCK_SET"); break;
    case APP_EVENT_CONFIG_CHANGED: COMMAND_OUT(context, "CONFIG_CHANGED"); break;
    case APP_EVENT_SETPOINT_MISMATCH: COMMAND_OUT(context, "SETPOINT_MISMATCH"); break;
    default: COMMAND_OUT(context, "UNKNOWN"); break;
    }
}

static void OutputMode(CommandContext *context, uint8_t mode)
{
    switch (mode)
    {
    case 0: COMMAND_OUT(context, "NORMAL"); break;
    case 1: COMMAND_OUT(context, "MANUAL"); break;
    case 2: COMMAND_OUT(context, "WARNING"); break;
    case 3: COMMAND_OUT(context, "ERROR"); break;
    default: COMMAND_OUT(context, "UNKNOWN"); break;
    }
}

static void OutputUnsigned(CommandContext *context, uint16_t value)
{
    char digits[6];
    uint8_t count = 0U;
    char out[2] = {0, '\0'};
    do
    {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    while (count > 0U)
    {
        out[0] = digits[--count];
        Command_OutputString(context, out);
    }
}

static void OutputSigned(CommandContext *context, int16_t value)
{
    if (value < 0)
    {
        COMMAND_OUT(context, "-");
        OutputUnsigned(context, (uint16_t)(-(int32_t)value));
    }
    else
    {
        OutputUnsigned(context, (uint16_t)value);
    }
}

int Cmd_event(CommandContext *context, int argc, char *argv[])
{
    uint8_t slot;
    uint8_t shown = 0U;
    AppEvent event;

    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 2 && strcmp(argv[1], "get") != 0) return CMDLINE_INVALID_ARG;

    for (slot = 0U; slot < APP_EVENT_SLOTS; slot++)
    {
        if (!AppEvent_Read(slot, &event)) continue;
        shown++;
        COMMAND_OUT(context, "EVENT seq=");
        OutputUnsigned(context, event.seq);
        COMMAND_OUT(context, " sev=");
        if (event.severity == APP_EVENT_SEVERITY_WARNING) COMMAND_OUT(context, "WARNING");
        else if (event.severity == APP_EVENT_SEVERITY_INFO) COMMAND_OUT(context, "INFO");
        else COMMAND_OUT(context, "OTHER");
        COMMAND_OUT(context, " code=");
        OutputCode(context, event.code);
        COMMAND_OUT(context, " mode=");
        OutputMode(context, event.mode);
        COMMAND_OUT(context, " value=");
        OutputSigned(context, event.value);
        COMMAND_OUT(context, " time=");
        Command_OutputClock(context, event.time_s);
        COMMAND_OUT(context, " source=");
        Command_OutputClockSource(context, event.clock_source);
        COMMAND_OUT(context, "\r\n");
    }
    if (shown == 0U) COMMAND_OUT(context, "EVENT none\r\n");
    return CMDLINE_OK;
}
