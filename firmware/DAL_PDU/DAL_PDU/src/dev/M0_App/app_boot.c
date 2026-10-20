#include "app_boot.h"

#include <stdbool.h>

#include "app_watchdog.h"
#include "app_watchdog_note.h"
#include "../M2_BSP/NVM/bsp_nvm.h"
#include "../M2_BSP/Reset/bsp_reset.h"

static AppBoot_Info s_info;

static void AppBoot_RecordHistory(const BSP_NVM_BootRecord *record)
{
    uint8_t slot = BSP_NVM_BootHistoryNext();
    BSP_NVM_WriteBootHistory(slot, record);
    BSP_NVM_SetBootHistoryNext((uint8_t)((slot + 1U) % BSP_NVM_HISTORY_SLOTS));
}

static uint8_t CauseFromFlags(uint8_t flags)
{
    if ((flags & BSP_RESET_POWER_ON) != 0U) return APP_BOOT_CAUSE_POWER_ON;
    if ((flags & BSP_RESET_BROWN_OUT) != 0U) return APP_BOOT_CAUSE_BROWN_OUT;
    if ((flags & BSP_RESET_EXTERNAL) != 0U) return APP_BOOT_CAUSE_EXTERNAL;
    if ((flags & BSP_RESET_WATCHDOG) != 0U) return APP_BOOT_CAUSE_AVR_WATCHDOG;
    return APP_BOOT_CAUSE_UNKNOWN;
}

void AppBoot_Init(void)
{
    uint8_t flags = BSP_Reset_CaptureFlags();
    bool requested = BSP_Reset_TakeSoftwareRequest();
    uint8_t source = APP_WATCHDOG_SOURCE_NONE;
    bool noted = AppWatchdogNote_Take(&source);
    BSP_NVM_BootRecord previous;
    BSP_NVM_BootRecord next;

    /* The TPL5010 resets the MCU through RST, which sets the external reset flag. A watchdog
     * note without that flag is stale, so only the flag decides a client-watchdog reset. */
    if (!BSP_NVM_ReadBootRecord(&previous)) previous.boot_count = 0U;
    if (noted && (flags & BSP_RESET_EXTERNAL) != 0U) next.cause = APP_BOOT_CAUSE_CLIENT_WATCHDOG;
    else if (requested && (flags & BSP_RESET_WATCHDOG) != 0U) next.cause = APP_BOOT_CAUSE_COMMAND;
    else next.cause = CauseFromFlags(flags);
    next.source = (next.cause == APP_BOOT_CAUSE_CLIENT_WATCHDOG) ? source : APP_WATCHDOG_SOURCE_NONE;
    next.boot_count = (uint16_t)(previous.boot_count + 1U);
    BSP_NVM_WriteBootRecord(&next);
    AppBoot_RecordHistory(&next);

    s_info.boot_count = next.boot_count;
    s_info.cause = next.cause;
    s_info.source = next.source;
}

AppBoot_Info AppBoot_Get(void)
{
    return s_info;
}

bool AppBoot_History(uint8_t index, BSP_NVM_BootRecord *record)
{
    uint8_t newest;
    if (index >= BSP_NVM_HISTORY_SLOTS || record == 0) return false;
    newest = (uint8_t)((BSP_NVM_BootHistoryNext() + BSP_NVM_HISTORY_SLOTS - 1U) % BSP_NVM_HISTORY_SLOTS);
    return BSP_NVM_ReadBootHistory(
        (uint8_t)((newest + BSP_NVM_HISTORY_SLOTS - index) % BSP_NVM_HISTORY_SLOTS), record);
}
