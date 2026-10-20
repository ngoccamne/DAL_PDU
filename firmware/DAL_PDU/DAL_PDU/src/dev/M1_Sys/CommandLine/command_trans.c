#include "command_trans.h"
#include "command.h"
#include "cmdline.h"
#include "../SystemData/system_data.h"
#include "../../M0_App/app_power.h"

#include <string.h>

int Cmd_trans(CommandContext *context, int argc, char *argv[])
{
    SystemData_PowerStatus status;
    if (argc < 2) return CMDLINE_TOO_FEW_ARGS;
    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
    if (strcmp(argv[1], "on") == 0)
    {
        if (!AppPower_SetTrans(true))
        {
            COMMAND_OUT(context, "TRANS fault\r\n");
            return CMDLINE_OK;
        }
    }
    else if (strcmp(argv[1], "off") == 0)
    {
        AppPower_SetTrans(false);
    }
    else if (strcmp(argv[1], "get") != 0)
    {
        return CMDLINE_INVALID_ARG;
    }
    status = SystemData_GetPowerStatus();
    if (status.trans_on) COMMAND_OUT(context, "TRANS on");
    else COMMAND_OUT(context, "TRANS off");
    if (status.fault_pc4) COMMAND_OUT(context, " fault");
    COMMAND_OUT(context, "\r\n");
    return CMDLINE_OK;
}
