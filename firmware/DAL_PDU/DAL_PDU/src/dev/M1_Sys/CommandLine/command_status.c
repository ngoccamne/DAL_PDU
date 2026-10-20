#include "command_status.h"

#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "command_bootinfo.h"
#include "command_fault.h"
#include "command_mode.h"
#include "command_telemetry.h"
#include "command_temp.h"

/* One reply with the sections the technician checks first: mode and causes, temperature,
 * setpoint and rails, the fault latch, and the last boot. Each section uses the command's own
 * output, so the formats match. */
int Cmd_status(CommandContext *context, int argc, char *argv[])
{
    char name[] = "status";
    char *args[] = {name};
    (void)argv;

    if (argc > 1) return CMDLINE_TOO_MANY_ARGS;
    COMMAND_OUT(context, "STATUS\r\n");
    (void)Cmd_mode(context, 1, args);
    (void)Cmd_temp(context, 1, args);
    (void)Cmd_telemetry(context, 1, args);
    (void)Cmd_fault(context, 1, args);
    (void)Cmd_bootinfo(context, 1, args);
    return CMDLINE_OK;
}
