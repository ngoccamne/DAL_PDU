#include "command_persist.h"

#include <string.h>

#include "cmdline.h"
#include "../../M0_App/app_power.h"
#include "../../M2_BSP/EEPROM/bsp_eeprom.h"
#include "../../M2_BSP/NVM/bsp_nvm.h"
#include "../../M2_BSP/Reset/bsp_reset.h"

/* persist_reset internal confirm: blanks application EEPROM records (fault latch,
 * limits, boot record/history, config, clock checkpoint), preserving bootloader
 * journal/request bytes, then restarts the MCU the same
 * way `reset confirm` does, since all of that is cached in RAM at boot and needs a fresh
 * init to pick up the now-blank EEPROM.
 *
 * persist_reset external confirm: blanks the U14 24C04 (the event log ring). Nothing
 * reads it into RAM at boot, so this does not restart the MCU or touch the outputs. */
int Cmd_persist_reset(CommandContext *context, int argc, char *argv[])
{
    bool internal;
    if (argc > 3) return CMDLINE_TOO_MANY_ARGS;
    if (argc < 3) return CMDLINE_TOO_FEW_ARGS;
    if (strcmp(argv[1], "internal") == 0) internal = true;
    else if (strcmp(argv[1], "external") == 0) internal = false;
    else return CMDLINE_INVALID_ARG;
    if (strcmp(argv[2], "confirm") != 0) return CMDLINE_INVALID_ARG;

    if (internal)
    {
        AppPower_TurnOffOutputs();
        BSP_NVM_EraseAll();
        COMMAND_OUT(context, "PERSIST_RESET internal EEPROM erased, restarting\r\n");
        BSP_Reset_Software();
        return CMDLINE_OK;
    }

    if (BSP_EEPROM_EraseAll())
        COMMAND_OUT(context, "PERSIST_RESET external EEPROM erased\r\n");
    else
        COMMAND_OUT(context, "PERSIST_RESET external EEPROM failed\r\n");
    return CMDLINE_OK;
}
