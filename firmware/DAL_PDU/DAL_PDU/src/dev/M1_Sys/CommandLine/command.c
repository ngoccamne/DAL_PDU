/*
 * command.c
 *
 */
#include "command.h"
#include "../Version/version.h"
#include "cmdline.h"
#include "command_led.h"
#include "command_trans.h"
#include "command_i2c.h"
#include "command_tps55287.h"
#include "command_setpoint.h"
#include "command_bootinfo.h"
#include "command_pac1934.h"
#include "command_temp.h"
#include "command_telemetry.h"
#include "command_fault.h"
#include "command_mode.h"
#include "command_limit.h"
#include "command_config.h"
#include "command_status.h"
#include "command_time.h"
#include "command_reset.h"
#include "command_event.h"
#include "command_eeprom.h"
#include "command_persist.h"

void Command_OutputString(CommandContext *context, const char *text)
{
    if (context != 0 && context->write != 0 && text != 0)
    {
        context->write(context->user, text);
    }
}

#ifdef __AVR__
void Command_OutputFlashString(CommandContext *context, PGM_P text)
{
    char chunk[16];
    uint8_t count = 0U;
    char value;
    do
    {
        value = (char)pgm_read_byte(text++);
        if (value != '\0') chunk[count++] = value;
        if (value == '\0' || count == sizeof(chunk) - 1U)
        {
            chunk[count] = '\0';
            Command_OutputString(context, chunk);
            count = 0U;
        }
    } while (value != '\0');
}
#define OUTPUT_CMD_TEXT(context, text) Command_OutputFlashString((context), (text))
#else
#define OUTPUT_CMD_TEXT(context, text) Command_OutputString((context), (text))
#endif

void Command_ContinueOutput(CommandContext *context)
{
    const tCmdLineEntry *entry;
    if (context == 0 || !context->help_pending) return;
    entry = &g_psCmdTable[context->help_index];
    if (entry->pcCmd == 0)
    {
        context->help_pending = false;
        return;
    }
    OUTPUT_CMD_TEXT(context, entry->pcCmd);
    OUTPUT_CMD_TEXT(context, entry->pcHelp);
    COMMAND_OUT(context, "\r\n");
    context->help_index++;
    if (g_psCmdTable[context->help_index].pcCmd == 0)
        context->help_pending = false;
}

/*******************************************************************************
 * Format: help
 ******************************************************************************/
static int Cmd_help(CommandContext *context, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    COMMAND_OUT(context, "\r\nAvailable commands\r\n");
    COMMAND_OUT(context, "------------------\r\n");
    if (context != 0)
    {
        context->help_index = 0U;
        context->help_pending = true;
    }

    return CMDLINE_OK;
}

/*******************************************************************************
 * Format: version
 ******************************************************************************/
static int Cmd_version(CommandContext *context, int argc, char *argv[])
{
    (void)argv;
    if (argc > 1) return CMDLINE_TOO_MANY_ARGS;

    COMMAND_OUT(context, "DAL_PDU firmware v" FIRMWARE_VERSION " (" FIRMWARE_STATUS ")"
                                  " build " FIRMWARE_BUILD_ID "\r\n");

    return CMDLINE_OK;
}

#define CMD_DEFINE(id, name, help) \
    static const char id##_name[] CMD_TEXT_STORAGE = name; \
    static const char id##_help[] CMD_TEXT_STORAGE = help;
CMD_DEFINE(help, "help", " : Display list of commands, format: help")
CMD_DEFINE(version, "version", " : Display firmware version, format: version")
CMD_DEFINE(led, "led", " : LED timing, format: led get | led set <on_ms> <off_ms>")
CMD_DEFINE(trans, "trans", " : TRANS power, format: trans on | trans off | trans get")
CMD_DEFINE(i2cdetect, "i2cdetect", " : Scan I2C bus, format: i2cdetect [first last]")
CMD_DEFINE(i2cget, "i2cget", " : Read I2C byte, format: i2cget <addr> [reg]")
CMD_DEFINE(i2cset, "i2cset", " : Write I2C byte, format: i2cset <addr> <reg> <value>")
CMD_DEFINE(tps55287, "tps55287", " : U5 VOUT, format: tps55287 status | get | set <mV> | on | off")
CMD_DEFINE(setpoint, "setpoint", " : Commanded VOUT setpoint from system data, format: setpoint get")
CMD_DEFINE(pac1934, "pac1934", " : Rail monitor, format: pac1934 [get | status | refresh]")
CMD_DEFINE(fault, "fault", " : Fault status, format: fault [get | clear | clear all]")
CMD_DEFINE(limit, "limit", " : Limits, format: limit [get | temp <tenths C> | vout <mA> | trans <mA>]")
CMD_DEFINE(config, "config", " : Config, format: config [get | set address <1-247> | set baud <bps> | set timeout <1-60 s> | set retry_enable <0|1> | set retry_time <1-60 s>]")
CMD_DEFINE(config_reset, "config_reset", " : Reset config and limits to defaults, format: config_reset confirm")
CMD_DEFINE(event, "event", " : Event log, format: event [get]")
CMD_DEFINE(mode, "mode", " : Operating mode, format: mode [get | normal | manual | retry]")
CMD_DEFINE(temp, "temp", " : Board temperature, format: temp [get | refresh]")
CMD_DEFINE(telemetry, "telemetry", " : Setpoint and rail telemetry, format: telemetry")
CMD_DEFINE(bootinfo, "bootinfo", " : Boot count, last cause, and history, format: bootinfo [get | history]")
CMD_DEFINE(time, "time", " : Clock, format: time [get | set <unix seconds>]")
CMD_DEFINE(status, "status", " : Status summary, format: status")
CMD_DEFINE(reset, "reset", " : Restart the MCU with outputs off, format: reset confirm")
CMD_DEFINE(eeprom, "eeprom", " : U14 24C04, format: eeprom probe | read <addr> [len] | write <addr> <hexbytes>")
CMD_DEFINE(persist_reset, "persist_reset", " : Erase EEPROM to defaults, format: persist_reset internal confirm | persist_reset external confirm")


tCmdLineEntry g_psCmdTable[] =
{
    { help_name, Cmd_help, help_help },
    { version_name, Cmd_version, version_help },
    { led_name, Cmd_led, led_help },
    { trans_name, Cmd_trans, trans_help },
    { i2cdetect_name, Cmd_i2cdetect, i2cdetect_help },
    { i2cget_name, Cmd_i2cget, i2cget_help },
    { i2cset_name, Cmd_i2cset, i2cset_help },
    { tps55287_name, Cmd_tps55287, tps55287_help },
    { setpoint_name, Cmd_setpoint, setpoint_help },
    { pac1934_name, Cmd_pac1934, pac1934_help },
    { fault_name, Cmd_fault, fault_help },
    { limit_name, Cmd_limit, limit_help },
    { config_name, Cmd_config, config_help },
    { config_reset_name, Cmd_config_reset, config_reset_help },
    { event_name, Cmd_event, event_help },
    { mode_name, Cmd_mode, mode_help },
    { temp_name, Cmd_temp, temp_help },
    { telemetry_name, Cmd_telemetry, telemetry_help },
    { bootinfo_name, Cmd_bootinfo, bootinfo_help },
    { time_name, Cmd_time, time_help },
    { status_name, Cmd_status, status_help },
    { reset_name, Cmd_reset, reset_help },
    { eeprom_name, Cmd_eeprom, eeprom_help },
    { persist_reset_name, Cmd_persist_reset, persist_reset_help },
    { 0, 0, 0 }
};
