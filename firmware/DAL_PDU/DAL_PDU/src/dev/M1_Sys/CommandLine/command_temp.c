#include "command_temp.h"

#include <string.h>

#include "cmdline.h"
#include "../SystemData/system_data.h"
#include "../../M0_App/app_temperature.h"

static void OutputTenthsC(CommandContext *context, int16_t tenths_c)
{
    char text[8];
    uint16_t magnitude = (tenths_c < 0) ? (uint16_t)-tenths_c : (uint16_t)tenths_c;
    uint8_t count = 0U;
    uint8_t index;

    text[count++] = (char)('0' + magnitude % 10U);
    magnitude /= 10U;
    text[count++] = '.';
    do
    {
        text[count++] = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude != 0U);
    if (tenths_c < 0) text[count++] = '-';

    for (index = 0U; index < count / 2U; index++)
    {
        char swap = text[index];
        text[index] = text[count - index - 1U];
        text[count - index - 1U] = swap;
    }
    text[count] = '\0';
    Command_OutputString(context, text);
}

int Cmd_temp(CommandContext *context, int argc, char *argv[])
{
    SystemData_TemperatureStatus status;

    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 2)
    {
        if (strcmp(argv[1], "refresh") == 0) AppTemperature_Update();
        else if (strcmp(argv[1], "get") != 0) return CMDLINE_INVALID_ARG;
    }

    status = SystemData_GetTemperatureStatus();
    if (!status.valid)
    {
        COMMAND_OUT(context, "TEMP unavailable\r\n");
        return CMDLINE_OK;
    }
    COMMAND_OUT(context, "TEMP ");
    OutputTenthsC(context, status.tenths_c);
    COMMAND_OUT(context, " C\r\n");
    return CMDLINE_OK;
}
