#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_event.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_mode.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_setpoint.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Scheduler/scheduler.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/TPS55287/bsp_tps55287.h"

static unsigned failures;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

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

/* The commanded record, as the BSP reports it. */
static BSP_TPS55287_Status fake_tps;
BSP_TPS55287_Status BSP_TPS55287_GetStatus(void)
{
    return fake_tps;
}

/* VOUT enable as the regulator reports it. */
static bool vout_on;
bool BSP_TPS55287_IsEnabled(bool *enabled)
{
    *enabled = vout_on;
    return true;
}

static void advance_ms(uint32_t ms)
{
    while (ms-- != 0U) SCH_RunSystemTickTimer();
}

/* Sets the state the check reads. The setpoint is the commanded one, measured is the PAC VOUT. */
static void set_state(bool trans, bool vout, uint16_t setpoint_mv, bool pac_valid, uint16_t measured_mv)
{
    BSP_TPS55287_Status tps;
    SystemData_PowerStatus power = {trans, false, false, false, false};
    SystemData_PowerMonitorStatus monitor;
    memset(&tps, 0, sizeof(tps));
    memset(&monitor, 0, sizeof(monitor));
    tps.setpoint_valid = true;
    tps.setpoint_mv = setpoint_mv;
    monitor.valid = pac_valid;
    monitor.voltage_mv[0] = measured_mv;
    fake_tps = tps;
    SystemData_SetPowerStatus(power);
    SystemData_SetPowerMonitorStatus(monitor);
    vout_on = vout;
}

/* One rail-monitor sample, one second after the last. */
static void step(void)
{
    advance_ms(1000U);
    AppSetpoint_Update();
}

static void reset_fixture(void)
{
    SystemData_Init();
    SCH_Initialize();
    SCH_StartScheduler();
    AppMode_SelfTest(true, true, true);
    memset(eeprom, 0xFF, sizeof(eeprom));
    vout_on = false;
    AppSetpoint_Update();
}

/* Starts a judged output: the first call restarts the settling time. */
static void start(uint16_t setpoint_mv, uint16_t measured_mv)
{
    set_state(true, true, setpoint_mv, true, measured_mv);
    AppSetpoint_Update();
}

static void test_matching_output_never_flags(void)
{
    reset_fixture();
    start(5000U, 5000U);
    step(); step(); step(); step(); step();
    CHECK(!AppMode_SetpointMismatch());
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
    CHECK(!AppEvent_Read(0U, &(AppEvent){0}));
}

static void test_mismatch_flags_after_settle_and_three_samples(void)
{
    AppEvent event;
    reset_fixture();
    start(5000U, 3000U);
    step();                          /* settling */
    CHECK(!AppMode_SetpointMismatch());
    step();                          /* first judged sample */
    CHECK(!AppMode_SetpointMismatch());
    step();                          /* second */
    CHECK(!AppMode_SetpointMismatch());
    step();                          /* third: flagged */
    CHECK(AppMode_SetpointMismatch());
    CHECK(AppMode_Get() == APP_MODE_WARNING);
    CHECK(AppEvent_Read(0U, &event));
    CHECK(event.severity == APP_EVENT_SEVERITY_WARNING && event.code == APP_EVENT_SETPOINT_MISMATCH);
    CHECK(event.value == 3000);
    step();                          /* still flagged, logged once */
    CHECK(AppMode_SetpointMismatch());
    CHECK(!AppEvent_Read(1U, &event));
}

static void test_tolerance_is_five_percent(void)
{
    reset_fixture();
    start(5000U, 5240U);             /* 240 mV off, inside 250 mV */
    step(); step(); step(); step(); step();
    CHECK(!AppMode_SetpointMismatch());

    set_state(true, true, 5000U, true, 5260U); /* 260 mV off, outside */
    step(); step(); step();
    CHECK(AppMode_SetpointMismatch());
}

static void test_small_setpoint_uses_the_floor(void)
{
    reset_fixture();
    start(1000U, 1090U);             /* 5 % is 50 mV, so the 100 mV floor applies */
    step(); step(); step(); step(); step();
    CHECK(!AppMode_SetpointMismatch());

    set_state(true, true, 1000U, true, 1110U); /* 110 mV off, outside */
    step(); step(); step();
    CHECK(AppMode_SetpointMismatch());
}

static void test_good_sample_clears_the_flag(void)
{
    reset_fixture();
    start(5000U, 3000U);
    step(); step(); step(); step();
    CHECK(AppMode_SetpointMismatch());
    set_state(true, true, 5000U, true, 5000U);
    step();
    CHECK(!AppMode_SetpointMismatch());
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
}

static void test_setpoint_change_restarts_settling(void)
{
    reset_fixture();
    start(5000U, 3000U);
    step(); step(); step(); step();
    CHECK(AppMode_SetpointMismatch());

    /* A new setpoint clears the flag at once and waits for the settling time again. */
    set_state(true, true, 4000U, true, 3000U);
    AppSetpoint_Update();
    CHECK(!AppMode_SetpointMismatch());
    step();
    CHECK(!AppMode_SetpointMismatch());
    step(); step(); step();
    CHECK(AppMode_SetpointMismatch());
}

static void test_no_check_while_outputs_are_off(void)
{
    reset_fixture();
    set_state(true, false, 5000U, true, 0U);     /* VOUT disabled */
    AppSetpoint_Update();
    step(); step(); step(); step(); step();
    CHECK(!AppMode_SetpointMismatch());

    set_state(false, true, 5000U, true, 0U);     /* TRANS off */
    step(); step(); step(); step(); step();
    CHECK(!AppMode_SetpointMismatch());

    set_state(true, true, 5000U, false, 0U);     /* PAC1934 unreadable */
    step(); step(); step(); step(); step();
    CHECK(!AppMode_SetpointMismatch());
}

static void test_leaving_the_check_clears_the_flag(void)
{
    reset_fixture();
    start(5000U, 3000U);
    step(); step(); step(); step();
    CHECK(AppMode_SetpointMismatch());
    vout_on = false;
    set_state(true, false, 5000U, true, 3000U);
    step();
    CHECK(!AppMode_SetpointMismatch());
}

int main(void)
{
    test_matching_output_never_flags();
    test_mismatch_flags_after_settle_and_three_samples();
    test_tolerance_is_five_percent();
    test_small_setpoint_uses_the_floor();
    test_good_sample_clears_the_flag();
    test_setpoint_change_restarts_settling();
    test_no_check_while_outputs_are_off();
    test_leaving_the_check_clears_the_flag();

    if (failures != 0U)
    {
        fprintf(stderr, "%u setpoint check(s) failed\n", failures);
        return 1;
    }
    puts("setpoint tests passed");
    return 0;
}
