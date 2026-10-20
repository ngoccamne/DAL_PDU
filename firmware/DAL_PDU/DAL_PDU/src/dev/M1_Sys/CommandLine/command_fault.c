#include "command_fault.h"

#include <string.h>

#include "cmdline.h"
#include "../SystemData/system_data.h"
#include "../../M0_App/app_limits.h"
#include "../../M0_App/app_mode.h"
#include "../../M0_App/app_power.h"
#include "../../M0_App/app_power_monitor.h"
#include "../../M2_BSP/EEPROM/bsp_eeprom.h"
#include "../../M2_BSP/I2C/bsp_i2c.h"

static void OutputFlag(CommandContext *context, bool value)
{
    if (value) COMMAND_OUT(context, "1");
    else COMMAND_OUT(context, "0");
}

int Cmd_fault(CommandContext *context, int argc, char *argv[])
{
    SystemData_PowerStatus status;

    if (argc > 3) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 3)
    {
        bool efuse_cleared;
        bool errors_cleared = true;
        if (strcmp(argv[1], "clear") != 0 || strcmp(argv[2], "all") != 0)
            return CMDLINE_INVALID_ARG;

        AppPowerMonitor_Update();
        AppMode_SetEepromMissing(!BSP_EEPROM_Probe());
        efuse_cleared = AppPower_ClearFaultLatch();
        if (AppMode_RegulatorError() || AppMode_LimitError())
            errors_cleared = efuse_cleared && !AppMode_PacUnreadable() &&
                             !AppLimits_Active() && AppLimits_Retry(AppPower_ProbeRegulator());
        AppMode_SetBusFault(BSP_I2C_BusFault());
        AppMode_RestartLinkTimeout();

        if (efuse_cleared && errors_cleared && !AppMode_PacUnreadable() &&
            !AppMode_EepromMissing() && !AppMode_SetpointMismatch() &&
            !AppMode_BusFault() && !AppMode_LinkLost())
            COMMAND_OUT(context, "FAULT clear all complete\r\n");
        else
            COMMAND_OUT(context, "FAULT clear all incomplete; run mode get and fault get\r\n");
        return CMDLINE_OK;
    }
    if (argc == 2 && strcmp(argv[1], "clear") == 0)
    {
        if (AppMode_LimitError() && AppLimits_Active())
        {
            COMMAND_OUT(context, "FAULT clear refused: limit still active\r\n");
            return CMDLINE_OK;
        }
        if (!AppPower_ClearFaultLatch())
        {
            COMMAND_OUT(context, "FAULT clear refused: fault still active\r\n");
            return CMDLINE_OK;
        }
        if (AppMode_LimitError() || AppMode_RegulatorError())
        {
            if (!AppLimits_Retry(AppPower_ProbeRegulator()))
            {
                COMMAND_OUT(context, "FAULT clear refused: regulator or limit unhealthy\r\n");
                return CMDLINE_OK;
            }
        }
        COMMAND_OUT(context, "FAULT cleared\r\n");
        return CMDLINE_OK;
    }
    if (argc == 2 && strcmp(argv[1], "get") != 0) return CMDLINE_INVALID_ARG;

    status = SystemData_GetPowerStatus();
    COMMAND_OUT(context, "FAULT latch pc4=");
    OutputFlag(context, status.latch_pc4);
    COMMAND_OUT(context, " pc0=");
    OutputFlag(context, status.latch_pc0);
    COMMAND_OUT(context, " active pc4=");
    OutputFlag(context, status.fault_pc4);
    COMMAND_OUT(context, " pc0=");
    OutputFlag(context, status.fault_pc0);
    COMMAND_OUT(context, "\r\n");
    return CMDLINE_OK;
}
