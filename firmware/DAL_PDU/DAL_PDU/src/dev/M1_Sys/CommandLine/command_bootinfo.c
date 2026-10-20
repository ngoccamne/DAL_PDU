#include "command_bootinfo.h"

#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "../../M0_App/app_boot.h"
#include "../../M0_App/app_watchdog.h"

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

static void OutputCause(CommandContext *context, uint8_t cause)
{
    switch (cause)
    {
    case APP_BOOT_CAUSE_POWER_ON: COMMAND_OUT(context, "POWER_ON"); break;
    case APP_BOOT_CAUSE_BROWN_OUT: COMMAND_OUT(context, "BROWN_OUT"); break;
    case APP_BOOT_CAUSE_EXTERNAL: COMMAND_OUT(context, "EXTERNAL"); break;
    case APP_BOOT_CAUSE_AVR_WATCHDOG: COMMAND_OUT(context, "AVR_WATCHDOG"); break;
    case APP_BOOT_CAUSE_CLIENT_WATCHDOG: COMMAND_OUT(context, "CLIENT_WATCHDOG"); break;
    case APP_BOOT_CAUSE_COMMAND: COMMAND_OUT(context, "COMMAND"); break;
    default: COMMAND_OUT(context, "UNKNOWN"); break;
    }
}

static void OutputSource(CommandContext *context, uint8_t source)
{
    switch (source)
    {
    case APP_WATCHDOG_SOURCE_NONE: COMMAND_OUT(context, "-"); break;
    case APP_WATCHDOG_SOURCE_CONSOLE: COMMAND_OUT(context, "CONSOLE"); break;
    case APP_WATCHDOG_SOURCE_HEARTBEAT: COMMAND_OUT(context, "HEARTBEAT"); break;
    case APP_WATCHDOG_SOURCE_MODBUS: COMMAND_OUT(context, "MODBUS"); break;
    case APP_WATCHDOG_SOURCE_POWER_MONITOR: COMMAND_OUT(context, "POWER_MONITOR"); break;
    case APP_WATCHDOG_SOURCE_TEMPERATURE: COMMAND_OUT(context, "TEMPERATURE"); break;
    case APP_WATCHDOG_SOURCE_MONITOR: COMMAND_OUT(context, "MONITOR"); break;
    default: COMMAND_OUT(context, "UNKNOWN"); break;
    }
}

static void OutputBoot(CommandContext *context, uint16_t count, uint8_t cause, uint8_t source)
{
    COMMAND_OUT(context, "BOOT count=");
    OutputUnsigned(context, count);
    COMMAND_OUT(context, " cause=");
    OutputCause(context, cause);
    COMMAND_OUT(context, " source=");
    OutputSource(context, source);
    COMMAND_OUT(context, "\r\n");
}

/* bootinfo shows the last boot. bootinfo history lists the stored boots, newest first. */
int Cmd_bootinfo(CommandContext *context, int argc, char *argv[])
{
    AppBoot_Info info;
    BSP_NVM_BootRecord record;
    uint8_t index;

    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 2 && strcmp(argv[1], "history") == 0)
    {
        for (index = 0U; AppBoot_History(index, &record); index++)
        {
            OutputBoot(context, record.boot_count, record.cause, record.source);
        }
        if (index == 0U) COMMAND_OUT(context, "BOOT history empty\r\n");
        return CMDLINE_OK;
    }
    if (argc == 2 && strcmp(argv[1], "get") != 0) return CMDLINE_INVALID_ARG;

    info = AppBoot_Get();
    OutputBoot(context, info.boot_count, info.cause, info.source);
    return CMDLINE_OK;
}
