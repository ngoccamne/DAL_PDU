#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_clock.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_event.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_mode.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/cmdline.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_time.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Scheduler/scheduler.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"

static unsigned failures;

extern unsigned fake_clock_writes;
void FakeClockNvm_Reset(void);

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

/* Console capture. */
static char console[256];
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

static int RunArgs(int (*command)(CommandContext *, int, char **), int argc, char **args)
{
    CommandContext context;
    memset(&context, 0, sizeof(context));
    console_len = 0U;
    console[0] = '\0';
    return command(&context, argc, args);
}

#define RUN(command, ...) RunArgs((command), \
    (int)(sizeof((char *[]){__VA_ARGS__}) / sizeof(char *)), (char *[]){__VA_ARGS__})

/* 24C04 stand-in for the event log. */
static uint8_t eeprom[512];
bool BSP_EEPROM_Read(uint16_t address, uint8_t *data, uint16_t length)
{
    if ((uint32_t)address + length > sizeof(eeprom)) return false;
    memcpy(data, &eeprom[address], length);
    return true;
}

bool BSP_EEPROM_Write(uint16_t address, const uint8_t *data, uint16_t length)
{
    if ((uint32_t)address + length > sizeof(eeprom)) return false;
    memcpy(&eeprom[address], data, length);
    return true;
}

static void advance_ms(uint32_t ms)
{
    while (ms-- != 0U) SCH_RunSystemTickTimer();
}

static void reset_fixture(void)
{
    FakeClockNvm_Reset();
    AppClock_Init();
    SystemData_Init();
    SCH_Initialize();
    SCH_StartScheduler();
    AppMode_SelfTest(true, true, true);
    memset(eeprom, 0xFF, sizeof(eeprom));
}

static void test_unset_until_set(void)
{
    reset_fixture();
    CHECK(AppClock_Now() == APP_CLOCK_UNSET);
    advance_ms(5000U);
    CHECK(AppClock_Now() == APP_CLOCK_UNSET);
    CHECK(!AppClock_Set(APP_CLOCK_UNSET));
    CHECK(AppClock_Now() == APP_CLOCK_UNSET);
}

static void test_time_advances_with_the_tick(void)
{
    reset_fixture();
    CHECK(AppClock_Set(1791290096UL));
    CHECK(AppClock_Now() == 1791290096UL);
    advance_ms(999U);
    CHECK(AppClock_Now() == 1791290096UL);
    advance_ms(1U);
    CHECK(AppClock_Now() == 1791290097UL);
    advance_ms(60000U);
    CHECK(AppClock_Now() == 1791290157UL);
}

static void test_calendar_conversion(void)
{
    AppClock_Calendar c;

    AppClock_ToCalendar(0UL, &c);
    CHECK(c.year == 1970 && c.month == 1 && c.day == 1 && c.hour == 0 && c.minute == 0 && c.second == 0);

    AppClock_ToCalendar(951782400UL, &c); /* 2000-02-29, a leap day */
    CHECK(c.year == 2000 && c.month == 2 && c.day == 29 && c.hour == 0);

    AppClock_ToCalendar(1700000000UL, &c);
    CHECK(c.year == 2023 && c.month == 11 && c.day == 14 && c.hour == 22 && c.minute == 13 && c.second == 20);

    AppClock_ToCalendar(1791290096UL, &c);
    CHECK(c.year == 2026 && c.month == 10 && c.day == 6 && c.hour == 12 && c.minute == 34 && c.second == 56);
}

static void test_time_command(void)
{
    char get[] = "time";
    char set[] = "set";
    char value[] = "1700000000";
    char *get_args[] = {get};
    char *set_args[] = {get, set, value};

    reset_fixture();
    CHECK(RUN(Cmd_time, "time") == CMDLINE_OK);
    CHECK(strcmp(console, "TIME unset source=UNSET\r\n") == 0);

    CHECK(RunArgs(Cmd_time, 3, set_args) == CMDLINE_OK);
    CHECK(strcmp(console, "TIME 2023-11-14T22:13:20Z source=SET\r\n") == 0);
    CHECK(AppClock_Now() == 1700000000UL);
    /* The set is logged, and stamped with the time it sets. */
    {
        AppEvent event;
        CHECK(AppEvent_Find(0U, &event));
        CHECK(event.code == APP_EVENT_CLOCK_SET && event.severity == APP_EVENT_SEVERITY_INFO);
        CHECK(event.time_s == 1700000000UL);
    }

    CHECK(RUN(Cmd_time, "time", "get") == CMDLINE_OK);
    CHECK(strcmp(console, "TIME 2023-11-14T22:13:20Z source=SET\r\n") == 0);
    CHECK(RUN(Cmd_time, "time", "set", "bad") == CMDLINE_INVALID_ARG);
    CHECK(RUN(Cmd_time, "time", "set", "17x") == CMDLINE_INVALID_ARG);
    CHECK(RUN(Cmd_time, "time", "set", "4294967296") == CMDLINE_INVALID_ARG);
    CHECK(RUN(Cmd_time, "time", "set", "4294967295") == CMDLINE_OK);
    CHECK(strcmp(console, "TIME refused: reserved value\r\n") == 0);
    CHECK(AppClock_Now() == 1700000000UL);
    CHECK(RUN(Cmd_time, "time", "reset") == CMDLINE_INVALID_ARG);
    CHECK(RunArgs(Cmd_time, 1, get_args) == CMDLINE_OK);
}

static void test_event_find_by_sequence(void)
{
    AppEvent event;
    reset_fixture();
    CHECK(!AppEvent_Find(0U, &event));
    CHECK(AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_TEMP_LIMIT, 900));
    CHECK(AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_VOUT_CURRENT, 3100));
    CHECK(AppEvent_Find(1U, &event) && event.code == APP_EVENT_TEMP_LIMIT && event.value == 900);
    CHECK(AppEvent_Find(2U, &event) && event.code == APP_EVENT_VOUT_CURRENT);
    CHECK(AppEvent_Find(0U, &event) && event.seq == 2U); /* newest */
    CHECK(!AppEvent_Find(3U, &event));
    CHECK(event.time_s == APP_CLOCK_UNSET); /* the clock was never set */
}

static void test_event_time_is_recorded(void)
{
    AppEvent event;
    reset_fixture();
    CHECK(AppClock_Set(1700000000UL));
    advance_ms(2000U);
    CHECK(AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_TEMP_LIMIT, 850));
    CHECK(AppEvent_Find(1U, &event));
    CHECK(event.time_s == 1700000002UL);
}

static void test_event_wrap_keeps_newest(void)
{
    AppEvent event;
    reset_fixture();
    /* Header at sequence 0xFFFE, so the next two events are 0xFFFF and then 0x0000. */
    eeprom[0] = 0x5FU;
    eeprom[1] = 0U;
    eeprom[2] = 0xFEU;
    eeprom[3] = 0xFFU;
    CHECK(AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_TEMP_LIMIT, 1));
    CHECK(AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_TEMP_LIMIT, 2));
    CHECK(AppEvent_Find(0xFFFFU, &event) && event.value == 1);
    CHECK(AppEvent_Find(0U, &event) && event.seq == 0U && event.value == 2); /* 0 is newer than 0xFFFF */
}

static void test_source_and_restore(void)
{
    reset_fixture();
    CHECK(AppClock_Source() == APP_CLOCK_SOURCE_UNSET);
    CHECK(AppClock_Set(1791290096UL));
    CHECK(AppClock_Source() == APP_CLOCK_SOURCE_SET);
    CHECK(fake_clock_writes == 1U); /* A set stores a checkpoint at once. */
    AppClock_Init(); /* A reset loses the RAM clock, but the checkpoint stays. */
    CHECK(AppClock_Source() == APP_CLOCK_SOURCE_RESTORED);
    CHECK(AppClock_Now() == 1791290096UL);
}

static void test_checkpoint_every_fifteen_minutes(void)
{
    reset_fixture();
    CHECK(AppClock_Set(1000000UL));
    advance_ms(899000U);
    AppClock_Update();
    CHECK(fake_clock_writes == 1U);
    advance_ms(1000U);
    AppClock_Update();
    CHECK(fake_clock_writes == 2U);
    AppClock_Update();
    CHECK(fake_clock_writes == 2U);
}

static void test_restore_takes_latest_write_not_largest(void)
{
    reset_fixture();
    CHECK(AppClock_Set(1791290096UL));
    CHECK(AppClock_Set(946684800UL)); /* set backwards */
    AppClock_Init();
    CHECK(AppClock_Now() == 946684800UL && AppClock_Source() == APP_CLOCK_SOURCE_RESTORED);
}

static void test_no_checkpoint_while_unset(void)
{
    reset_fixture();
    advance_ms(2000000U);
    AppClock_Update();
    CHECK(fake_clock_writes == 0U);
    CHECK(AppClock_Source() == APP_CLOCK_SOURCE_UNSET);
}

static void test_event_records_clock_source(void)
{
    AppEvent event;
    reset_fixture();
    CHECK(AppClock_Set(1700000000UL));
    CHECK(AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_TEMP_LIMIT, 1));
    CHECK(AppEvent_Find(0U, &event));
    CHECK(event.clock_source == APP_CLOCK_SOURCE_SET && event.mode == APP_MODE_NORMAL);
    AppClock_Init();
    CHECK(AppEvent_Log(APP_EVENT_SEVERITY_WARNING, APP_EVENT_TEMP_LIMIT, 2));
    CHECK(AppEvent_Find(0U, &event));
    CHECK(event.clock_source == APP_CLOCK_SOURCE_RESTORED && event.value == 2);
}

int main(void)
{
    test_unset_until_set();
    test_time_advances_with_the_tick();
    test_calendar_conversion();
    test_time_command();
    test_event_find_by_sequence();
    test_event_time_is_recorded();
    test_event_wrap_keeps_newest();
    test_source_and_restore();
    test_checkpoint_every_fifteen_minutes();
    test_restore_takes_latest_write_not_largest();
    test_no_checkpoint_while_unset();
    test_event_records_clock_source();

    if (failures != 0U)
    {
        fprintf(stderr, "%u clock checks failed\n", failures);
        return 1;
    }
    puts("clock tests passed");
    return 0;
}
