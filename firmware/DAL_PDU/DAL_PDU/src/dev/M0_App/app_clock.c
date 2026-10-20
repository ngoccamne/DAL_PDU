#include "app_clock.h"

#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M2_BSP/NVM/bsp_nvm.h"

#define SECONDS_PER_DAY 86400UL
#define DAYS_TO_CIVIL_OFFSET 719468UL /* Days from 0000-03-01 to 1970-01-01. */
#define DAYS_PER_ERA 146097UL

static bool s_set;
static uint8_t s_source;
static uint32_t s_base_s;
static uint32_t s_base_tick_ms;
static uint32_t s_last_checkpoint_s;

/* Writes the next checkpoint slot and advances the ring. */
static void Checkpoint(uint32_t unix_s)
{
    uint8_t next = BSP_NVM_ClockNext();
    BSP_NVM_WriteClockSlot(next, unix_s);
    BSP_NVM_SetClockNext((uint8_t)((next + 1U) % BSP_NVM_CLOCK_SLOTS));
    s_last_checkpoint_s = unix_s;
}

void AppClock_Init(void)
{
    uint8_t latest = (uint8_t)((BSP_NVM_ClockNext() + BSP_NVM_CLOCK_SLOTS - 1U) % BSP_NVM_CLOCK_SLOTS);
    uint32_t stored;
    s_set = false;
    s_source = APP_CLOCK_SOURCE_UNSET;
    if (!BSP_NVM_ReadClockSlot(latest, &stored)) return;
    s_base_s = stored;
    s_base_tick_ms = SCH_SystemTick();
    s_last_checkpoint_s = stored;
    s_set = true;
    s_source = APP_CLOCK_SOURCE_RESTORED;
}

bool AppClock_Set(uint32_t unix_s)
{
    if (unix_s == APP_CLOCK_UNSET) return false;
    s_base_s = unix_s;
    s_base_tick_ms = SCH_SystemTick();
    s_set = true;
    s_source = APP_CLOCK_SOURCE_SET;
    Checkpoint(unix_s);
    return true;
}

void AppClock_Update(void)
{
    uint32_t now;
    if (!s_set) return;
    now = AppClock_Now();
    if (now - s_last_checkpoint_s >= APP_CLOCK_CHECKPOINT_S) Checkpoint(now);
}

uint32_t AppClock_Now(void)
{
    uint32_t elapsed_ms;
    if (!s_set) return APP_CLOCK_UNSET;
    /* Unsigned subtraction stays right across the tick counter's wrap. */
    elapsed_ms = SCH_SystemTick() - s_base_tick_ms;
    return s_base_s + elapsed_ms / 1000UL;
}

uint8_t AppClock_Source(void)
{
    return s_source;
}

/* Converts days since 1970-01-01 to a civil date (proleptic Gregorian calendar). */
void AppClock_ToCalendar(uint32_t unix_s, AppClock_Calendar *calendar)
{
    uint32_t days;
    uint32_t seconds_of_day;
    uint32_t z;
    uint32_t era;
    uint32_t day_of_era;
    uint32_t year_of_era;
    uint32_t day_of_year;
    uint32_t month_index;
    uint32_t year;

    if (calendar == 0) return;
    days = unix_s / SECONDS_PER_DAY;
    seconds_of_day = unix_s % SECONDS_PER_DAY;

    z = days + DAYS_TO_CIVIL_OFFSET;
    era = z / DAYS_PER_ERA;
    day_of_era = z - era * DAYS_PER_ERA;
    year_of_era = (day_of_era - day_of_era / 1460UL + day_of_era / 36524UL - day_of_era / 146096UL) / 365UL;
    year = year_of_era + era * 400UL;
    day_of_year = day_of_era - (365UL * year_of_era + year_of_era / 4UL - year_of_era / 100UL);
    month_index = (5UL * day_of_year + 2UL) / 153UL;
    calendar->day = (uint8_t)(day_of_year - (153UL * month_index + 2UL) / 5UL + 1UL);
    calendar->month = (uint8_t)((month_index < 10UL) ? month_index + 3UL : month_index - 9UL);
    if (calendar->month <= 2U) year++;
    calendar->year = (uint16_t)year;

    calendar->hour = (uint8_t)(seconds_of_day / 3600UL);
    calendar->minute = (uint8_t)((seconds_of_day % 3600UL) / 60UL);
    calendar->second = (uint8_t)(seconds_of_day % 60UL);
}
