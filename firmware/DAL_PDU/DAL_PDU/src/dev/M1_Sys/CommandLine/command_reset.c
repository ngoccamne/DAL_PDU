#include "command_reset.h"

#include <string.h>

#include "cmdline.h"
#include "../../M0_App/app_power.h"
#include "../../M2_BSP/Reset/bsp_reset.h"

/* reset confirm: turns both outputs off, then restarts the MCU through the watchdog. The next
 * boot records the cause as COMMAND. */
int Cmd_reset(CommandContext *context, int argc, char *argv[])
{
    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
    if (argc != 2 || strcmp(argv[1], "confirm") != 0) return CMDLINE_INVALID_ARG;

    AppPower_TurnOffOutputs();
    COMMAND_OUT(context, "RESET outputs off, restarting\r\n");
    BSP_Reset_Software();
    return CMDLINE_OK;
}
