#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_mode.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_power.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Scheduler/scheduler.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NVM/bsp_nvm.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Power/bsp_power.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/TPS55287/bsp_tps55287.h"

static unsigned failures;
static bool trans_on;
static bool fault_pc4;
static bool fault_pc0;
static bool vout_enabled;
static bool regulator_present;
static uint8_t nvm_bits;
static unsigned nvm_writes;

void AppRetry_NoteShutdown(void)
{
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

void BSP_Power_Init(void)
{
    trans_on = false;
}

bool BSP_Power_SetTrans(bool enable)
{
    /* Matches the real bsp_power.c: does not check the live PC4/PC0 pins. */
    trans_on = enable;
    return true;
}

bool BSP_Power_IsTransOn(void)
{
    return trans_on;
}

BSP_PowerFaults BSP_Power_ReadFaults(void)
{
    BSP_PowerFaults faults = {fault_pc4, fault_pc0};
    return faults;
}

bool BSP_TPS55287_SetEnabled(bool enabled)
{
    vout_enabled = enabled;
    return true;
}

bool BSP_TPS55287_Detect(void)
{
    return regulator_present;
}

bool BSP_TPS55287_SetInternalFeedback(void)
{
    return true;
}

uint8_t BSP_NVM_ReadFaultLatch(void)
{
    return nvm_bits;
}

void BSP_NVM_WriteFaultLatch(uint8_t bits)
{
    nvm_bits = bits;
    nvm_writes++;
}

static void reset_fixture(uint8_t stored_bits)
{
    trans_on = false;
    fault_pc4 = false;
    fault_pc0 = false;
    vout_enabled = false;
    regulator_present = true;
    nvm_bits = stored_bits;
    nvm_writes = 0U;
    SystemData_Init();
    SCH_Initialize();
    SCH_StartScheduler(); /* So SCH_SystemTick() advances for the eFuse debounce timers. */
}

static void advance_ms(uint32_t ms)
{
    while (ms-- != 0U) SCH_RunSystemTickTimer();
}

static void test_fault_latches_and_persists(void)
{
    SystemData_PowerStatus status;

    reset_fixture(0U);
    CHECK(AppPower_Init());
    CHECK(AppPower_SetTrans(true));
    vout_enabled = true;

    fault_pc4 = true;
    AppPower_Update(); /* Starts the 50 ms debounce timer; not yet confirmed. */
    CHECK(trans_on);
    advance_ms(50U);
    AppPower_Update(); /* Confirmed: now latches. */
    CHECK(trans_on && vout_enabled);
    CHECK(nvm_bits == BSP_NVM_FAULT_PC4 && nvm_writes == 1U);
    status = SystemData_GetPowerStatus();
    CHECK(status.latch_pc4 && !status.latch_pc0);
    CHECK(AppMode_Get() == APP_MODE_WARNING);

    fault_pc4 = false;
    AppPower_Update();
    AppPower_Update();
    CHECK(nvm_writes == 1U);

    CHECK(AppPower_SetTrans(true));
    CHECK(trans_on && vout_enabled);
}

static void test_clear_reenables(void)
{
    SystemData_PowerStatus status;

    reset_fixture(BSP_NVM_FAULT_PC0);
    AppPower_Init();
    status = SystemData_GetPowerStatus();
    CHECK(status.latch_pc0 && !status.latch_pc4);
    CHECK(AppPower_SetTrans(true));

    CHECK(AppPower_ClearFaultLatch());
    CHECK(nvm_bits == 0U && nvm_writes == 1U);
    status = SystemData_GetPowerStatus();
    CHECK(!status.latch_pc0 && !status.latch_pc4);
    CHECK(AppPower_SetTrans(true) && trans_on);
}

static void test_clear_while_fault_active(void)
{
    reset_fixture(BSP_NVM_FAULT_PC4);
    AppPower_Init();
    fault_pc0 = true;
    CHECK(!AppPower_ClearFaultLatch());
    CHECK(nvm_bits == BSP_NVM_FAULT_PC4);
    CHECK(AppPower_SetTrans(true) && trans_on);
    fault_pc0 = false;
    CHECK(AppPower_ClearFaultLatch());
    CHECK(nvm_bits == 0U);
}

static void test_error_blocks_trans_warning_allows(void)
{
    reset_fixture(0U);
    AppMode_SelfTest(false, true, true);
    AppPower_Init();
    CHECK(!AppPower_SetTrans(true) && !trans_on);
    AppMode_SelfTest(true, false, true);
    CHECK(AppPower_SetTrans(true) && trans_on);
    AppPower_SetTrans(false);
}

static void test_damaged_record_reports_warning(void)
{
    reset_fixture(BSP_NVM_FAULT_PC4 | BSP_NVM_FAULT_PC0);
    AppPower_Init();
    CHECK(AppPower_SetTrans(true));
    CHECK(SystemData_GetPowerStatus().latch_pc4 && SystemData_GetPowerStatus().latch_pc0);
    CHECK(AppMode_Get() == APP_MODE_WARNING);
}

static void test_live_fault_before_enable_is_no_longer_refused(void)
{
    reset_fixture(0U);
    AppPower_Init();
    fault_pc0 = true;
    /* A live fault does not block the enable attempt. */
    CHECK(AppPower_SetTrans(true) && trans_on);
    CHECK(nvm_writes == 0U && nvm_bits == 0U);
    /* The poll still catches it after 50 ms, but does not shut TRANS off. */
    AppPower_Update();
    advance_ms(50U);
    AppPower_Update();
    CHECK(trans_on);
    CHECK(nvm_bits == BSP_NVM_FAULT_PC0 && nvm_writes == 1U);
}

static void test_mcu_fault_latches_with_trans_off(void)
{
    SystemData_PowerStatus status;
    reset_fixture(0U);
    AppPower_Init();
    fault_pc0 = true;
    AppPower_Update();
    advance_ms(50U);
    AppPower_Update();
    CHECK(nvm_bits == BSP_NVM_FAULT_PC0 && nvm_writes == 1U);
    status = SystemData_GetPowerStatus();
    CHECK(status.fault_pc0 && status.latch_pc0 && !status.latch_pc4);
    CHECK(AppPower_SetTrans(true) && trans_on);
    AppPower_Update();
    CHECK(nvm_writes == 1U);
}

static void test_mcu_fault_keeps_outputs_on(void)
{
    reset_fixture(0U);
    AppPower_Init();
    CHECK(AppPower_SetTrans(true) && trans_on);
    vout_enabled = true;
    fault_pc0 = true;
    AppPower_Update();
    advance_ms(50U);
    AppPower_Update();
    CHECK(trans_on && vout_enabled);
    CHECK(nvm_bits == BSP_NVM_FAULT_PC0);
}

static void test_trans_fault_with_unpowered_regulator_keeps_trans_on(void)
{
    reset_fixture(0U);
    AppPower_Init();
    fault_pc4 = true;
    regulator_present = false;
    CHECK(AppPower_SetTrans(true) && trans_on);
    CHECK(!AppMode_RegulatorError());
    AppPower_Update();
    advance_ms(50U);
    AppPower_Update();
    CHECK(trans_on && nvm_bits == BSP_NVM_FAULT_PC4);
    CHECK(AppMode_Get() == APP_MODE_WARNING);
}

static void test_brief_faults_do_not_latch(void)
{
    reset_fixture(0U);
    AppPower_Init();
    CHECK(AppPower_SetTrans(true));
    fault_pc0 = true;
    fault_pc4 = true;
    AppPower_Update();
    advance_ms(24U);
    fault_pc0 = false;
    fault_pc4 = false;
    AppPower_Update();
    advance_ms(50U);
    AppPower_Update();
    CHECK(nvm_bits == 0U && trans_on);
}

static void test_manual_mode_reports_fault_without_shutdown(void)
{
    reset_fixture(0U);
    AppPower_Init();
    CHECK(AppPower_SetTrans(true));
    CHECK(AppMode_RequestManual());
    vout_enabled = true;
    fault_pc0 = true;
    AppPower_Update();
    advance_ms(50U);
    AppPower_Update();
    CHECK(nvm_bits == BSP_NVM_FAULT_PC0);
    CHECK(AppMode_Get() == APP_MODE_MANUAL);
    CHECK(trans_on && vout_enabled);
    CHECK(!AppPower_ClearFaultLatch());
    fault_pc0 = false;
    AppPower_Update();
    CHECK(AppPower_ClearFaultLatch());
    CHECK(AppMode_RequestNormal());
}

static void test_absent_regulator_at_enable_sets_error(void)
{
    reset_fixture(0U);
    AppPower_Init();
    regulator_present = false;
    CHECK(!AppPower_SetTrans(true));
    CHECK(!trans_on);
    CHECK(AppMode_RegulatorError());

    /* The chip shows up later (e.g. once powered externally); ERROR still blocks trans on
     * until an explicit retry, matching every other latched ERROR cause. */
    regulator_present = true;
    CHECK(!AppPower_SetTrans(true));
    CHECK(!trans_on);
    CHECK(AppMode_Retry(true, true));
    CHECK(AppPower_SetTrans(true) && trans_on);
}

static void test_probe_regulator_for_retry(void)
{
    /* A latched eFuse WARNING does not block the regulator probe. */
    reset_fixture(BSP_NVM_FAULT_PC4);
    AppPower_Init();
    CHECK(AppPower_ProbeRegulator());
    CHECK(!trans_on);

    /* TRANS starts off; a present regulator is probed and TRANS is restored to off. */
    reset_fixture(0U);
    AppPower_Init();
    CHECK(!trans_on);
    CHECK(AppPower_ProbeRegulator());
    CHECK(!trans_on);

    /* An absent regulator also leaves TRANS off afterward, not stuck on. */
    regulator_present = false;
    CHECK(!AppPower_ProbeRegulator());
    CHECK(!trans_on);
    regulator_present = true;

    /* A persisted limit error, unrelated to the regulator, can now be cleared by retry
     * even though TRANS starts off: the probe powers TRANS just long enough to check,
     * where AppMode_Get() != APP_MODE_ERROR would otherwise have refused to power it. */
    AppMode_SetLimitError();
    CHECK(AppMode_Retry(AppPower_ProbeRegulator(), true));
    CHECK(!AppMode_LimitError());
    CHECK(!trans_on);
}

static void test_manual_mode_keeps_trans_on_despite_regulator_error(void)
{
    reset_fixture(0U);
    AppPower_Init();
    CHECK(AppPower_SetTrans(true) && trans_on);

    AppMode_SetRegulatorFault();
    CHECK(AppMode_Get() == APP_MODE_ERROR);
    AppPower_Update();
    CHECK(!trans_on); /* Baseline: ERROR still turns TRANS off via the 6 ms poll. */

    /* MANUAL outranks ERROR (decided 2026-10-15): trans on is allowed again, and the
     * periodic poll no longer turns it back off just because the regulator cause is
     * still set underneath. */
    CHECK(AppMode_RequestManual());
    CHECK(AppMode_Get() == APP_MODE_MANUAL);
    CHECK(AppPower_SetTrans(true) && trans_on);
    AppPower_Update();
    CHECK(trans_on);

    /* Clean up for any test that might run after this one. */
    CHECK(AppMode_Retry(true, true));
    CHECK(AppMode_RequestNormal());
}

int main(void)
{
    test_fault_latches_and_persists();
    test_clear_reenables();
    test_clear_while_fault_active();
    test_error_blocks_trans_warning_allows();
    test_damaged_record_reports_warning();
    test_live_fault_before_enable_is_no_longer_refused();
    test_mcu_fault_latches_with_trans_off();
    test_mcu_fault_keeps_outputs_on();
    test_trans_fault_with_unpowered_regulator_keeps_trans_on();
    test_brief_faults_do_not_latch();
    test_manual_mode_reports_fault_without_shutdown();
    test_absent_regulator_at_enable_sets_error();
    test_probe_regulator_for_retry();
    test_manual_mode_keeps_trans_on_despite_regulator_error();
    if (failures != 0U)
    {
        fprintf(stderr, "%u fault latch test failure(s)\n", failures);
        return 1;
    }
    printf("fault latch tests passed\n");
    return 0;
}
