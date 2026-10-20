#include "command_telemetry.h"

#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "../SystemData/system_data.h"
#include "../../M2_BSP/TPS55287/bsp_tps55287.h"

static void OutputUnsigned(CommandContext *context, uint32_t value)
{
    char digits[10];
    uint8_t count = 0U;
    char out[2] = {0, '\0'};
    do
    {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U && count < sizeof(digits));
    while (count > 0U)
    {
        out[0] = digits[--count];
        Command_OutputString(context, out);
    }
}

static void OutputRailName(CommandContext *context, uint8_t channel)
{
    switch (channel)
    {
    case 0: COMMAND_OUT(context, "VOUT"); break;
    case 1: COMMAND_OUT(context, "+12"); break;
    case 2: COMMAND_OUT(context, "+5_TRANS"); break;
    default: COMMAND_OUT(context, "+5"); break;
    }
}

int Cmd_telemetry(CommandContext *context, int argc, char *argv[])
{
    BSP_TPS55287_Status tps;
    SystemData_PowerMonitorStatus monitor;
    uint8_t channel;

    (void)argv;
    if (argc > 1) return CMDLINE_TOO_MANY_ARGS;

    tps = BSP_TPS55287_GetStatus();
    monitor = SystemData_GetPowerMonitorStatus();
    if (tps.setpoint_valid)
    {
        COMMAND_OUT(context, "SETPOINT ");
        OutputUnsigned(context, tps.setpoint_mv);
        COMMAND_OUT(context, " mV\r\n");
    }
    else
    {
        COMMAND_OUT(context, "SETPOINT unset\r\n");
    }

    if (!monitor.valid)
    {
        COMMAND_OUT(context, "PAC1934 unavailable\r\n");
        return CMDLINE_OK;
    }
    for (channel = 0U; channel < SYSTEM_DATA_POWER_CHANNELS; channel++)
    {
        OutputRailName(context, channel);
        COMMAND_OUT(context, " ");
        OutputUnsigned(context, monitor.voltage_mv[channel]);
        COMMAND_OUT(context, " mV ");
        OutputUnsigned(context, monitor.current_ma[channel]);
        COMMAND_OUT(context, " mA ");
        OutputUnsigned(context, monitor.power_mw[channel]);
        COMMAND_OUT(context, " mW\r\n");
    }
    return CMDLINE_OK;
}
