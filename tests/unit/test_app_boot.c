#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_boot.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_watchdog.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_watchdog_note.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/cmdline.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_bootinfo.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NVM/bsp_nvm.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Reset/bsp_reset.h"

static unsigned failures;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

/* Reset flags of the next boot. Capturing them clears them, as MCUSR does. */
static uint8_t next_flags;
uint8_t BSP_Reset_CaptureFlags(void)
{
    uint8_t flags = next_flags;
    next_flags = 0U;
    return flags;
}

/* Console reset request, kept across the reset. */
static bool reset_requested;
bool BSP_Reset_TakeSoftwareRequest(void)
{
    bool requested = reset_requested;
    reset_requested = false;
    return requested;
}

/* Boot record in internal EEPROM. */
static BSP_NVM_BootRecord nvm_record;
static bool nvm_record_valid;
bool BSP_NVM_ReadBootRecord(BSP_NVM_BootRecord *record)
{
    if (!nvm_record_valid) return false;
    *record = nvm_record;
    return true;
}

void BSP_NVM_WriteBootRecord(const BSP_NVM_BootRecord *record)
{
    nvm_record = *record;
    nvm_record_valid = true;
}

/* Console output for bootinfo. */
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

static BSP_NVM_BootRecord ring[BSP_NVM_HISTORY_SLOTS];
static bool ring_valid[BSP_NVM_HISTORY_SLOTS];
static uint8_t ring_next;

uint8_t BSP_NVM_BootHistoryNext(void)
{
    return ring_next;
}

void BSP_NVM_SetBootHistoryNext(uint8_t next)
{
    ring_next = next;
}

bool BSP_NVM_ReadBootHistory(uint8_t slot, BSP_NVM_BootRecord *record)
{
    if (slot >= BSP_NVM_HISTORY_SLOTS || !ring_valid[slot]) return false;
    *record = ring[slot];
    return true;
}

void BSP_NVM_WriteBootHistory(uint8_t slot, const BSP_NVM_BootRecord *record)
{
    ring[slot] = *record;
    ring_valid[slot] = true;
}

static void boot_with(uint8_t flags)
{
    next_flags = flags;
    AppBoot_Init();
}

static void reset_fixture(void)
{
    nvm_record_valid = false;
    memset(&nvm_record, 0, sizeof(nvm_record));
    next_flags = 0U;
    reset_requested = false;
    AppWatchdogNote_Take(0);
    memset(ring_valid, 0, sizeof(ring_valid));
    ring_next = 0U;
}

static void test_power_on_starts_count(void)
{
    AppBoot_Info info;
    reset_fixture();
    boot_with(BSP_RESET_POWER_ON);
    info = AppBoot_Get();
    CHECK(info.boot_count == 1U);
    CHECK(info.cause == APP_BOOT_CAUSE_POWER_ON);
    CHECK(info.source == APP_WATCHDOG_SOURCE_NONE);
}

static void test_each_reset_flag_maps_to_a_cause(void)
{
    reset_fixture();
    boot_with(BSP_RESET_EXTERNAL);
    CHECK(AppBoot_Get().cause == APP_BOOT_CAUSE_EXTERNAL);
    boot_with(BSP_RESET_BROWN_OUT);
    CHECK(AppBoot_Get().cause == APP_BOOT_CAUSE_BROWN_OUT);
    boot_with(BSP_RESET_WATCHDOG);
    CHECK(AppBoot_Get().cause == APP_BOOT_CAUSE_AVR_WATCHDOG);
    boot_with(0U);
    CHECK(AppBoot_Get().cause == APP_BOOT_CAUSE_UNKNOWN);
    CHECK(AppBoot_Get().boot_count == 4U);
}

static void test_console_reset_is_a_command_cause(void)
{
    reset_fixture();
    reset_requested = true;
    boot_with(BSP_RESET_WATCHDOG);
    CHECK(AppBoot_Get().cause == APP_BOOT_CAUSE_COMMAND);
    CHECK(AppBoot_Get().source == APP_WATCHDOG_SOURCE_NONE);
    /* The request is used once. The next watchdog reset is not a command. */
    boot_with(BSP_RESET_WATCHDOG);
    CHECK(AppBoot_Get().cause == APP_BOOT_CAUSE_AVR_WATCHDOG);
}

static void test_client_watchdog_reset_keeps_its_source(void)
{
    AppBoot_Info info;
    reset_fixture();
    boot_with(BSP_RESET_POWER_ON);
    AppWatchdogNote_Set(APP_WATCHDOG_SOURCE_MODBUS);
    /* The TPL5010 resets through RST, which sets the external reset flag. */
    boot_with(BSP_RESET_EXTERNAL);
    info = AppBoot_Get();
    CHECK(info.boot_count == 2U);
    CHECK(info.cause == APP_BOOT_CAUSE_CLIENT_WATCHDOG);
    CHECK(info.source == APP_WATCHDOG_SOURCE_MODBUS);
    CHECK(nvm_record.cause == APP_BOOT_CAUSE_CLIENT_WATCHDOG);
    CHECK(nvm_record.source == APP_WATCHDOG_SOURCE_MODBUS);

    /* The note is consumed. The next external reset is not a watchdog reset. */
    boot_with(BSP_RESET_EXTERNAL);
    CHECK(AppBoot_Get().cause == APP_BOOT_CAUSE_EXTERNAL);
    CHECK(AppBoot_Get().source == APP_WATCHDOG_SOURCE_NONE);
}

static void test_stale_note_without_external_flag_is_ignored(void)
{
    reset_fixture();
    AppWatchdogNote_Set(APP_WATCHDOG_SOURCE_POWER_MONITOR);
    boot_with(BSP_RESET_POWER_ON);
    CHECK(AppBoot_Get().cause == APP_BOOT_CAUSE_POWER_ON);
    CHECK(AppBoot_Get().source == APP_WATCHDOG_SOURCE_NONE);
    CHECK(!AppWatchdogNote_Take(0));
}

static void test_damaged_record_restarts_count(void)
{
    reset_fixture();
    boot_with(BSP_RESET_POWER_ON);
    boot_with(BSP_RESET_POWER_ON);
    CHECK(AppBoot_Get().boot_count == 2U);
    nvm_record_valid = false;
    boot_with(BSP_RESET_EXTERNAL);
    CHECK(AppBoot_Get().boot_count == 1U);
}

static void test_bootinfo_prints_count_and_cause(void)
{
    CommandContext context;
    char *args[] = {"bootinfo"};
    reset_fixture();
    boot_with(BSP_RESET_POWER_ON);
    AppWatchdogNote_Set(APP_WATCHDOG_SOURCE_MODBUS);
    boot_with(BSP_RESET_EXTERNAL);

    memset(&context, 0, sizeof(context));
    console_len = 0U;
    console[0] = '\0';
    CHECK(Cmd_bootinfo(&context, 1, args) == CMDLINE_OK);
    CHECK(strcmp(console, "BOOT count=2 cause=CLIENT_WATCHDOG source=MODBUS\r\n") == 0);
}

static void test_history_keeps_last_ten_newest_first(void)
{
    BSP_NVM_BootRecord record;
    unsigned i;
    reset_fixture();
    for (i = 0U; i < 12U; i++) boot_with(BSP_RESET_POWER_ON);
    boot_with(BSP_RESET_EXTERNAL);
    CHECK(AppBoot_History(0, &record) && record.boot_count == 13U);
    CHECK(record.cause == APP_BOOT_CAUSE_EXTERNAL);
    CHECK(AppBoot_History(9, &record) && record.boot_count == 4U);
    CHECK(!AppBoot_History(10, &record));
}

int main(void)
{
    test_power_on_starts_count();
    test_history_keeps_last_ten_newest_first();
    test_each_reset_flag_maps_to_a_cause();
    test_console_reset_is_a_command_cause();
    test_client_watchdog_reset_keeps_its_source();
    test_stale_note_without_external_flag_is_ignored();
    test_damaged_record_restarts_count();
    test_bootinfo_prints_count_and_cause();
    if (failures != 0U)
    {
        fprintf(stderr, "%u boot checks failed\n", failures);
        return 1;
    }
    puts("boot tests passed");
    return 0;
}
