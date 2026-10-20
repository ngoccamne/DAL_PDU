#include "command_setpoint.h"

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

/* Reports the commanded setpoint held in system data. It does not read the TPS55287. */
int Cmd_setpoint(CommandContext *context, int argc, char *argv[])
{
    BSP_TPS55287_Status tps;

    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 2 && strcmp(argv[1], "get") != 0) return CMDLINE_INVALID_ARG;

    tps = BSP_TPS55287_GetStatus();
    if (!tps.setpoint_valid)
    {
        COMMAND_OUT(context, "SETPOINT unset\r\n");
        return CMDLINE_OK;
    }
    COMMAND_OUT(context, "SETPOINT ");
    OutputUnsigned(context, tps.setpoint_mv);
    COMMAND_OUT(context, " mV\r\n");
    return CMDLINE_OK;
}
