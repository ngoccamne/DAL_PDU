#include "command_mode.h"

#include <string.h>

#include "cmdline.h"
#include "../SystemData/system_data.h"
#include "../../M0_App/app_limits.h"
#include "../../M0_App/app_mode.h"
#include "../../M0_App/app_power.h"
#include "../../M0_App/app_retry.h"

static void OutputFlag(CommandContext *context, bool value)
{
    if (value) COMMAND_OUT(context, "1");
    else COMMAND_OUT(context, "0");
}

int Cmd_mode(CommandContext *context, int argc, char *argv[])
{
    static const char *const names[] = {"normal", "manual", "warning", "error"};
    SystemData_PowerStatus power;
    AppMode mode;

    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 2)
    {
        if (strcmp(argv[1], "normal") == 0)
        {
            if (((AppMode_RegulatorError() || AppMode_LimitError()) &&
                 !AppLimits_Retry(AppPower_ProbeRegulator())) || !AppMode_RequestNormal())
                COMMAND_OUT(context, "MODE refused: error\r\n");
        }
        else if (strcmp(argv[1], "manual") == 0)
        {
            if (!AppMode_RequestManual()) COMMAND_OUT(context, "MODE refused: error\r\n");
        }
        else if (strcmp(argv[1], "retry") == 0)
        {
            if (!AppRetry_Attempt()) COMMAND_OUT(context, "MODE retry failed\r\n");
            else COMMAND_OUT(context, "MODE retry OK\r\n");
        }
        else if (strcmp(argv[1], "get") != 0)
        {
            return CMDLINE_INVALID_ARG;
        }
    }

    mode = AppMode_Get();
    power = SystemData_GetPowerStatus();
    COMMAND_OUT(context, "MODE ");
    Command_OutputString(context, names[mode]);
    COMMAND_OUT(context, "\r\nCAUSES regulator=");
    OutputFlag(context, AppMode_RegulatorError());
    COMMAND_OUT(context, " pac1934=");
    OutputFlag(context, AppMode_PacUnreadable());
    COMMAND_OUT(context, " eeprom=");
    OutputFlag(context, AppMode_EepromMissing());
    COMMAND_OUT(context, " latch=");
    OutputFlag(context, power.latch_pc4 || power.latch_pc0);
    COMMAND_OUT(context, " limit=");
    OutputFlag(context, AppMode_LimitError());
    COMMAND_OUT(context, " setpoint=");
    OutputFlag(context, AppMode_SetpointMismatch());
    COMMAND_OUT(context, " bus=");
    OutputFlag(context, AppMode_BusFault());
    COMMAND_OUT(context, " link=");
    OutputFlag(context, AppMode_LinkLost());
    COMMAND_OUT(context, "\r\n");
    return CMDLINE_OK;
}
