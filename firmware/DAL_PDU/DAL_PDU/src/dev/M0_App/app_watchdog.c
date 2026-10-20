#include "app_watchdog.h"

#include "app_watchdog_note.h"
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/Watchdog/bsp_watchdog.h"

#define WATCHDOG_MONITOR_TIMEOUT_MS 200UL
#define WATCHDOG_FEED_INTERVAL_MS 1000UL
#define WATCHDOG_DONE_PULSE_MS 100UL

typedef struct
{
    uint32_t last_checkin_ms;
    uint16_t timeout_ms;
    bool registered;
    bool seen;
} AppWatchdog_ClientState;

static AppWatchdog_ClientState s_clients[APP_WATCHDOG_CLIENT_COUNT];
static bool s_started;
static uint32_t s_last_update_ms;
static uint32_t s_last_done_ms;
static uint32_t s_done_since_ms;
static bool s_done_high;
static bool s_faulted;

static void AppWatchdog_Fault(uint8_t source)
{
    AppWatchdogNote_Set(source);
    s_faulted = true;
    s_done_high = false;
    BSP_Watchdog_Inhibit();
}

bool AppWatchdog_Init(void)
{
    uint8_t i;

    if (!BSP_Watchdog_Init()) return false;
    for (i = 0U; i < APP_WATCHDOG_CLIENT_COUNT; i++)
    {
        s_clients[i].registered = false;
        s_clients[i].seen = false;
        s_clients[i].last_checkin_ms = 0U;
        s_clients[i].timeout_ms = 0U;
    }
    s_started = false;
    s_last_update_ms = SCH_SystemTick();
    s_last_done_ms = SCH_SystemTick();
    s_done_since_ms = 0U;
    s_done_high = false;
    s_faulted = false;
    return true;
}

bool AppWatchdog_RegisterClient(AppWatchdog_Client client, uint16_t timeout_ms)
{
    AppWatchdog_ClientState *state;

    if (client >= APP_WATCHDOG_CLIENT_COUNT || timeout_ms == 0U || s_faulted)
        return false;
    state = &s_clients[client];
    if (state->registered) return false;
    state->last_checkin_ms = SCH_SystemTick();
    state->timeout_ms = timeout_ms;
    state->registered = true;
    state->seen = false;
    return true;
}

bool AppWatchdog_Start(void)
{
    uint8_t i;
    if (s_faulted || s_started) return false;
    for (i = 0U; i < APP_WATCHDOG_CLIENT_COUNT; i++)
    {
        if (!s_clients[i].registered) return false;
    }
    s_started = true;
    s_last_update_ms = SCH_SystemTick();
    s_last_done_ms = s_last_update_ms;
    return true;
}

void AppWatchdog_CheckIn(AppWatchdog_Client client)
{
    uint32_t now;
    AppWatchdog_ClientState *state;

    if (client >= APP_WATCHDOG_CLIENT_COUNT || s_faulted) return;
    state = &s_clients[client];
    if (!state->registered) return;
    now = SCH_SystemTick();
    if ((uint32_t)(now - state->last_checkin_ms) > state->timeout_ms)
    {
        AppWatchdog_Fault((uint8_t)(client + 1U));
        return;
    }
    state->last_checkin_ms = now;
    state->seen = true;
}

void AppWatchdog_Update(void)
{
    uint8_t i;
    uint32_t now = SCH_SystemTick();
    bool all_seen = true;
    if (s_faulted || !s_started) return;
    if ((uint32_t)(now - s_last_update_ms) > WATCHDOG_MONITOR_TIMEOUT_MS)
    {
        AppWatchdog_Fault(APP_WATCHDOG_SOURCE_MONITOR);
        return;
    }
    s_last_update_ms = now;

    for (i = 0U; i < APP_WATCHDOG_CLIENT_COUNT; i++)
    {
        AppWatchdog_ClientState *state = &s_clients[i];
        if (!state->registered ||
            (uint32_t)(now - state->last_checkin_ms) > state->timeout_ms)
        {
            AppWatchdog_Fault((uint8_t)(i + 1U));
            return;
        }
        if (!state->seen) all_seen = false;
    }
    if (!all_seen) return;

    if (s_done_high)
    {
        if ((uint32_t)(now - s_done_since_ms) >= WATCHDOG_DONE_PULSE_MS)
        {
            BSP_Watchdog_SetDone(false);
            s_done_high = false;
        }
        return; /* Ensure a low interval before any next rising edge. */
    }

    if ((uint32_t)(now - s_last_done_ms) >= WATCHDOG_FEED_INTERVAL_MS)
    {
        BSP_Watchdog_SetDone(true);
        s_done_high = true;
        s_done_since_ms = now;
        s_last_done_ms = now;
    }
}

bool AppWatchdog_IsFaulted(void)
{
    return s_faulted;
}
