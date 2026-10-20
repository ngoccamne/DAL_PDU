#include "command_pac1934.h"

#include <string.h>

#include "cmdline.h"
#include "../SystemData/system_data.h"
#include "../../M0_App/app_power_monitor.h"

static void OutputUnsigned(CommandContext *context, uint32_t value)
{
    char text[11];
    uint8_t count = 0U;
    uint8_t index;
    do
    {
        text[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    for (index = 0U; index < count / 2U; index++)
    {
        char swap = text[index];
        text[index] = text[count - index - 1U];
        text[count - index - 1U] = swap;
    }
    text[count] = '\0';
    Command_OutputString(context, text);
}

static void OutputAddress(CommandContext *context, uint8_t address)
{
    static const char digits[] = "0123456789abcdef";
    char text[5] = {'0', 'x', digits[address >> 4], digits[address & 0x0FU], '\0'};
    Command_OutputString(context, text);
}

int Cmd_pac1934(CommandContext *context, int argc, char *argv[])
{
    static const char *rail_names[SYSTEM_DATA_POWER_CHANNELS] =
    {
        "VOUT", "+12", "+5_TRANS", "+5"
    };
    SystemData_PowerMonitorStatus status;
    uint8_t channel;
    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 2)
    {
        if (strcmp(argv[1], "refresh") == 0) AppPowerMonitor_Update();
        else if (strcmp(argv[1], "get") != 0 && strcmp(argv[1], "status") != 0)
            return CMDLINE_INVALID_ARG;
    }
    status = SystemData_GetPowerMonitorStatus();
    if (!status.valid)
    {
        COMMAND_OUT(context, "PAC1934 unavailable\r\n");
        return CMDLINE_OK;
    }
    COMMAND_OUT(context, "PAC1934 addr=");
    OutputAddress(context, status.address);
    COMMAND_OUT(context, "\r\n");
    for (channel = 0U; channel < SYSTEM_DATA_POWER_CHANNELS; channel++)
    {
        Command_OutputString(context, rail_names[channel]);
        COMMAND_OUT(context, " ");
        OutputUnsigned(context, status.voltage_mv[channel]);
        COMMAND_OUT(context, " mV ");
        OutputUnsigned(context, status.current_ma[channel]);
        COMMAND_OUT(context, " mA ");
        OutputUnsigned(context, status.power_mw[channel]);
        COMMAND_OUT(context, " mW\r\n");
    }
    return CMDLINE_OK;
}
