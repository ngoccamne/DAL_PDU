#include "app_limits.h"

#include "app_event.h"
#include "app_mode.h"
#include "app_power.h"
#include "app_retry.h"
#include "../M1_Sys/SystemData/system_data.h"

#define CHANNEL_VOUT 0U
#define CHANNEL_5V_TRANS 2U

typedef struct
{
    uint8_t count;
    bool tripped;
} LimitTrack;

static BSP_NVM_Limits s_limits = {
    APP_LIMITS_TEMP_DEFAULT_TENTHS_C,
    APP_LIMITS_VOUT_DEFAULT_MA,
    APP_LIMITS_TRANS5V_DEFAULT_MA
};
static LimitTrack s_temp_track;
static LimitTrack s_vout_track;
static LimitTrack s_trans_track;

static bool CurrentInRange(uint32_t value)
{
    return value >= APP_LIMITS_CURRENT_MIN_MA && value <= APP_LIMITS_CURRENT_MAX_MA;
}

static void ReadOverLimits(bool *temp_over, bool *vout_over, bool *trans_over)
{
    SystemData_TemperatureStatus temperature = SystemData_GetTemperatureStatus();
    SystemData_PowerMonitorStatus monitor = SystemData_GetPowerMonitorStatus();
    *temp_over = temperature.valid && temperature.tenths_c >= s_limits.temp_trip_tenths_c;
    *vout_over = monitor.valid && monitor.current_ma[CHANNEL_VOUT] > s_limits.vout_max_ma;
    *trans_over = monitor.valid && monitor.current_ma[CHANNEL_5V_TRANS] > s_limits.trans5v_max_ma;
}

/* Counts consecutive over-limit samples. Returns true only on the sample that trips. */
static bool Track(LimitTrack *track, bool over, uint8_t samples)
{
    if (!over)
    {
        track->count = 0U;
        track->tripped = false;
        return false;
    }
    if (track->count < samples) track->count++;
    if (track->count >= samples && !track->tripped)
    {
        track->tripped = true;
        return true;
    }
    return false;
}

static void Trip(uint8_t code, int16_t value)
{
    AppEvent_Log(APP_EVENT_SEVERITY_WARNING, code, value);
    if (AppMode_Get() == APP_MODE_NORMAL)
    {
        AppRetry_NoteShutdown();
        AppPower_TurnOffOutputs();
        BSP_NVM_WriteLimitError(true);
        AppMode_SetLimitError();
    }
}

void AppLimits_Defaults(BSP_NVM_Limits *limits)
{
    if (limits == 0) return;
    limits->temp_trip_tenths_c = APP_LIMITS_TEMP_DEFAULT_TENTHS_C;
    limits->vout_max_ma = APP_LIMITS_VOUT_DEFAULT_MA;
    limits->trans5v_max_ma = APP_LIMITS_TRANS5V_DEFAULT_MA;
}

void AppLimits_Init(void)
{
    if (!BSP_NVM_ReadLimits(&s_limits)) AppLimits_Defaults(&s_limits);
    if (BSP_NVM_ReadLimitError()) AppMode_SetLimitError();
}

void AppLimits_Get(BSP_NVM_Limits *limits)
{
    if (limits != 0) *limits = s_limits;
}

bool AppLimits_Set(const BSP_NVM_Limits *limits)
{
    if (limits == 0) return false;
    if (limits->temp_trip_tenths_c < APP_LIMITS_TEMP_MIN_TENTHS_C ||
        limits->temp_trip_tenths_c > APP_LIMITS_TEMP_MAX_TENTHS_C) return false;
    if (!CurrentInRange(limits->vout_max_ma) || !CurrentInRange(limits->trans5v_max_ma)) return false;
    s_limits = *limits;
    BSP_NVM_WriteLimits(&s_limits);
    return true;
}

void AppLimits_Update(void)
{
    bool temp_over;
    bool vout_over;
    bool trans_over;
    SystemData_TemperatureStatus temperature = SystemData_GetTemperatureStatus();
    SystemData_PowerMonitorStatus monitor = SystemData_GetPowerMonitorStatus();

    ReadOverLimits(&temp_over, &vout_over, &trans_over);
    if (Track(&s_temp_track, temp_over, APP_LIMITS_TEMP_TRIP_SAMPLES))
    {
        Trip(APP_EVENT_TEMP_LIMIT, temperature.tenths_c);
    }
    if (Track(&s_vout_track, vout_over, APP_LIMITS_CURRENT_TRIP_SAMPLES))
    {
        Trip(APP_EVENT_VOUT_CURRENT, (int16_t)monitor.current_ma[CHANNEL_VOUT]);
    }
    if (Track(&s_trans_track, trans_over, APP_LIMITS_CURRENT_TRIP_SAMPLES))
    {
        Trip(APP_EVENT_TRANS_CURRENT, (int16_t)monitor.current_ma[CHANNEL_5V_TRANS]);
    }
}

bool AppLimits_Active(void)
{
    bool temp_over;
    bool vout_over;
    bool trans_over;
    ReadOverLimits(&temp_over, &vout_over, &trans_over);
    return temp_over || vout_over || trans_over;
}

bool AppLimits_Retry(bool regulator_ok)
{
    bool cleared = AppMode_Retry(regulator_ok, !AppLimits_Active());
    if (cleared) BSP_NVM_WriteLimitError(false);
    return cleared;
}
