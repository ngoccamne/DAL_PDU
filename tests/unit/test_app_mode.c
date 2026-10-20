#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_mode.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Scheduler/scheduler.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"

static unsigned failures;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

static void set_power(bool trans_on, bool latch_pc4, bool latch_pc0)
{
    SystemData_PowerStatus status = {trans_on, false, false, latch_pc4, latch_pc0};
    SystemData_SetPowerStatus(status);
}

static void advance_ms(uint32_t ms)
{
    while (ms-- != 0U) SCH_RunSystemTickTimer();
}

static void test_self_test_mapping(void)
{
    SystemData_Init();
    set_power(false, false, false);

    AppMode_SelfTest(true, true, true);
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
    CHECK(!AppMode_EepromMissing());

    AppMode_SelfTest(false, true, true);
    CHECK(AppMode_Get() == APP_MODE_ERROR);
    CHECK(AppMode_RegulatorError());

    AppMode_SelfTest(true, false, true);
    CHECK(AppMode_Get() == APP_MODE_WARNING);
    CHECK(AppMode_PacUnreadable());

    AppMode_SelfTest(true, true, false);
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
    CHECK(AppMode_EepromMissing());
}

static void test_priority_and_latch(void)
{
    SystemData_Init();
    AppMode_SelfTest(true, true, true);

    set_power(true, false, false);
    CHECK(AppMode_Get() == APP_MODE_NORMAL);

    /* A latched eFuse fault is WARNING and does not block NORMAL or TRANS. */
    set_power(false, true, false);
    CHECK(AppMode_Get() == APP_MODE_WARNING);
    CHECK(AppMode_RequestNormal());
    set_power(false, false, false);
    CHECK(AppMode_Get() == APP_MODE_NORMAL);

    CHECK(AppMode_RequestManual());
    CHECK(AppMode_Get() == APP_MODE_MANUAL);
    set_power(false, true, false);
    CHECK(AppMode_Get() == APP_MODE_MANUAL);
    CHECK(AppMode_RequestNormal()); /* Latch remains a WARNING underneath. */
    CHECK(AppMode_Get() == APP_MODE_WARNING);
    set_power(false, false, false);
    CHECK(AppMode_RequestNormal());
}

static void test_monitoring_and_retry(void)
{
    SystemData_Init();
    AppMode_SelfTest(true, true, true);
    CHECK(AppMode_RegulatorMonitored());

    CHECK(AppMode_RequestManual());
    CHECK(!AppMode_RegulatorMonitored());
    CHECK(AppMode_RequestNormal());

    AppMode_SetRegulatorFault();
    CHECK(AppMode_Get() == APP_MODE_ERROR);
    CHECK(!AppMode_RequestNormal());
    CHECK(!AppMode_Retry(false, true));
    CHECK(AppMode_Get() == APP_MODE_ERROR);
    CHECK(AppMode_Retry(true, true));
    CHECK(AppMode_Get() == APP_MODE_NORMAL);

    /* MANUAL outranks ERROR (decided 2026-10-15): reachable even while the automatic
     * retry path is stuck, so the operator can take manual control. */
    AppMode_SetRegulatorFault();
    CHECK(AppMode_Get() == APP_MODE_ERROR);
    CHECK(AppMode_RequestManual());
    CHECK(AppMode_Get() == APP_MODE_MANUAL);
    CHECK(!AppMode_RequestNormal()); /* Still refused: the cause itself isn't cleared yet. */
    CHECK(AppMode_Retry(true, true));
    CHECK(AppMode_Get() == APP_MODE_MANUAL); /* Retry clears the cause, not s_manual. */
    CHECK(AppMode_RequestNormal());
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
}

static void test_link_loss_needs_a_request_first(void)
{
    SCH_Initialize();
    SCH_StartScheduler();
    AppMode_SelfTest(true, true, true);
    advance_ms(10000U);
    CHECK(!AppMode_LinkLost());
    CHECK(AppMode_Get() == APP_MODE_NORMAL);

    AppMode_NoteRequest();
    advance_ms(APP_MODE_COMMS_TIMEOUT_MS);
    CHECK(!AppMode_LinkLost());
    advance_ms(1U);
    CHECK(AppMode_LinkLost());
    /* Link loss is a WARNING cause (decided 2026-10-15); it no longer turns outputs off
     * or changes mode priority (ERROR/MANUAL still win). */
    CHECK(AppMode_Get() == APP_MODE_WARNING);

    AppMode_NoteRequest();
    CHECK(!AppMode_LinkLost());
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
}

static void test_bus_fault_is_warning(void)
{
    AppMode_SelfTest(true, true, true);
    CHECK(AppMode_Retry(true, true));
    CHECK(AppMode_RequestNormal());
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
    AppMode_SetBusFault(true);
    CHECK(AppMode_BusFault());
    CHECK(AppMode_Get() == APP_MODE_WARNING);
    AppMode_SetBusFault(false);
    CHECK(AppMode_Get() == APP_MODE_NORMAL);
}

int main(void)
{
    test_self_test_mapping();
    test_bus_fault_is_warning();
    test_priority_and_latch();
    test_monitoring_and_retry();
    test_link_loss_needs_a_request_first();
    if (failures != 0U)
    {
        fprintf(stderr, "%u app mode test failure(s)\n", failures);
        return 1;
    }
    printf("app mode tests passed\n");
    return 0;
}
