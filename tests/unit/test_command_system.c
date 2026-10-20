#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/cmdline.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_reset.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_status.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_persist.h"

static unsigned failures;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

/* Console capture. */
static char console[512];
static size_t console_len;
void Command_OutputString(CommandContext *context, const char *text)
{
    size_t n = strlen(text);
    (void)context;
    if (console_len + n >= sizeof(console)) n = sizeof(console) - 1U - console_len;
    memcpy(&console[console_len], text, n);
    console_len += n;
    console[console_len] = '\0';
}

/* Records the order of the calls that matter for reset. */
static char sequence[8];
static unsigned sequence_len;
static void Note(char c)
{
    if (sequence_len < sizeof(sequence) - 1U) sequence[sequence_len++] = c;
    sequence[sequence_len] = '\0';
}

/* Stand-ins for the sections of status. Each records that it ran and with which argc. */
static int section_argc[5];
static unsigned section_calls;
static int Section(int index, CommandContext *context, int argc, const char *label)
{
    section_argc[index] = argc;
    section_calls++;
    Command_OutputString(context, label);
    return CMDLINE_OK;
}
int Cmd_mode(CommandContext *context, int argc, char *argv[]) { (void)argv; return Section(0, context, argc, "MODE\r\n"); }
int Cmd_temp(CommandContext *context, int argc, char *argv[]) { (void)argv; return Section(1, context, argc, "TEMP\r\n"); }
int Cmd_telemetry(CommandContext *context, int argc, char *argv[]) { (void)argv; return Section(2, context, argc, "TELEMETRY\r\n"); }
int Cmd_fault(CommandContext *context, int argc, char *argv[]) { (void)argv; return Section(3, context, argc, "FAULT\r\n"); }
int Cmd_bootinfo(CommandContext *context, int argc, char *argv[]) { (void)argv; return Section(4, context, argc, "BOOT\r\n"); }

/* Reset stand-ins. */
void AppPower_TurnOffOutputs(void)
{
    Note('O');
}

void BSP_Reset_Software(void)
{
    Note('R');
}

void BSP_NVM_EraseAll(void)
{
    Note('N');
}

static bool eeprom_erase_ok = true;
bool BSP_EEPROM_EraseAll(void)
{
    Note('E');
    return eeprom_erase_ok;
}

static int Run(int (*command)(CommandContext *, int, char **), char **args, int argc)
{
    CommandContext context;
    memset(&context, 0, sizeof(context));
    console_len = 0U;
    console[0] = '\0';
    return command(&context, argc, args);
}

static void test_status_runs_each_section_once(void)
{
    char name[] = "status";
    char *args[] = {name};
    int i;
    section_calls = 0U;
    CHECK(Run(Cmd_status, args, 1) == CMDLINE_OK);
    CHECK(strcmp(console, "STATUS\r\nMODE\r\nTEMP\r\nTELEMETRY\r\nFAULT\r\nBOOT\r\n") == 0);
    CHECK(section_calls == 5U);
    for (i = 0; i < 5; i++) CHECK(section_argc[i] == 1);
}

static void test_status_refuses_arguments(void)
{
    char name[] = "status";
    char extra[] = "x";
    char *args[] = {name, extra};
    section_calls = 0U;
    CHECK(Run(Cmd_status, args, 2) == CMDLINE_TOO_MANY_ARGS);
    CHECK(console[0] == '\0' && section_calls == 0U);
}

static void test_reset_needs_confirm(void)
{
    char name[] = "reset";
    char word[] = "confirm";
    char other[] = "yes";
    char *bare[] = {name};
    char *wrong[] = {name, other};
    char *good[] = {name, word};
    sequence_len = 0U;
    sequence[0] = '\0';
    CHECK(Run(Cmd_reset, bare, 1) == CMDLINE_INVALID_ARG);
    CHECK(Run(Cmd_reset, wrong, 2) == CMDLINE_INVALID_ARG);
    CHECK(sequence[0] == '\0');
    CHECK(Run(Cmd_reset, good, 2) == CMDLINE_OK);
}

static void test_reset_turns_outputs_off_before_restart(void)
{
    char name[] = "reset";
    char word[] = "confirm";
    char *args[] = {name, word};
    sequence_len = 0U;
    sequence[0] = '\0';
    CHECK(Run(Cmd_reset, args, 2) == CMDLINE_OK);
    CHECK(strcmp(sequence, "OR") == 0); /* outputs off, then restart */
    CHECK(strcmp(console, "RESET outputs off, restarting\r\n") == 0);
}

static void test_persist_reset_needs_confirm(void)
{
    char name[] = "persist_reset";
    char internal[] = "internal";
    char word[] = "confirm";
    char other[] = "yes";
    char bad_scope[] = "both";
    char *too_few[] = {name, internal};
    char *bad[] = {name, bad_scope, word};
    char *wrong[] = {name, internal, other};
    char *good[] = {name, internal, word};
    sequence_len = 0U;
    sequence[0] = '\0';
    CHECK(Run(Cmd_persist_reset, too_few, 2) == CMDLINE_TOO_FEW_ARGS);
    CHECK(Run(Cmd_persist_reset, bad, 3) == CMDLINE_INVALID_ARG);
    CHECK(Run(Cmd_persist_reset, wrong, 3) == CMDLINE_INVALID_ARG);
    CHECK(sequence[0] == '\0');
    CHECK(Run(Cmd_persist_reset, good, 3) == CMDLINE_OK);
}

static void test_persist_reset_internal_erases_then_restarts(void)
{
    char name[] = "persist_reset";
    char internal[] = "internal";
    char word[] = "confirm";
    char *args[] = {name, internal, word};
    sequence_len = 0U;
    sequence[0] = '\0';
    CHECK(Run(Cmd_persist_reset, args, 3) == CMDLINE_OK);
    CHECK(strcmp(sequence, "ONR") == 0); /* outputs off, NVM erased, restart; external untouched */
    CHECK(strcmp(console, "PERSIST_RESET internal EEPROM erased, restarting\r\n") == 0);
}

static void test_persist_reset_external_erases_without_restart(void)
{
    char name[] = "persist_reset";
    char external[] = "external";
    char word[] = "confirm";
    char *args[] = {name, external, word};
    sequence_len = 0U;
    sequence[0] = '\0';
    eeprom_erase_ok = true;
    CHECK(Run(Cmd_persist_reset, args, 3) == CMDLINE_OK);
    CHECK(strcmp(sequence, "E") == 0); /* EEPROM erased only; no outputs, no NVM, no restart */
    CHECK(strcmp(console, "PERSIST_RESET external EEPROM erased\r\n") == 0);
}

static void test_persist_reset_external_reports_failure_without_restart(void)
{
    char name[] = "persist_reset";
    char external[] = "external";
    char word[] = "confirm";
    char *args[] = {name, external, word};
    sequence_len = 0U;
    sequence[0] = '\0';
    eeprom_erase_ok = false;
    CHECK(Run(Cmd_persist_reset, args, 3) == CMDLINE_OK);
    CHECK(strcmp(sequence, "E") == 0);
    CHECK(strcmp(console, "PERSIST_RESET external EEPROM failed\r\n") == 0);
    eeprom_erase_ok = true;
}

int main(void)
{
    test_status_runs_each_section_once();
    test_status_refuses_arguments();
    test_reset_needs_confirm();
    test_reset_turns_outputs_off_before_restart();
    test_persist_reset_needs_confirm();
    test_persist_reset_internal_erases_then_restarts();
    test_persist_reset_external_erases_without_restart();
    test_persist_reset_external_reports_failure_without_restart();
    if (failures != 0U)
    {
        fprintf(stderr, "%u command system checks failed\n", failures);
        return 1;
    }
    puts("command system tests passed");
    return 0;
}
