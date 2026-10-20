#include "app_power.h"

#include "app_mode.h"
#include "app_retry.h"
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/NVM/bsp_nvm.h"
#include "../M2_BSP/Power/bsp_power.h"
#include "../M2_BSP/TPS55287/bsp_tps55287.h"

/* An eFuse fault pin must read continuously active for this long before it is treated as
 * real (decided 2026-10-16): a brief inrush-related glitch right after TRANS switches no
 * longer latches on a single noisy 6 ms sample. */
#define EFUSE_FAULT_DEBOUNCE_MS 50UL

static uint8_t s_latch;
static bool s_pc4_timing;
static uint32_t s_pc4_since_ms;
static bool s_pc0_timing;
static uint32_t s_pc0_since_ms;

/* Returns true once `active` has read continuously true for EFUSE_FAULT_DEBOUNCE_MS.
 * Any inactive reading resets the timer. */
static bool AppPower_DebounceActive(bool active, bool *timing, uint32_t *since_ms)
{
    uint32_t now = SCH_SystemTick();
    if (!active)
    {
        *timing = false;
        return false;
    }
    if (!*timing)
    {
        *timing = true;
        *since_ms = now;
    }
    return (uint32_t)(now - *since_ms) >= EFUSE_FAULT_DEBOUNCE_MS;
}

static void AppPower_PublishStatus(BSP_PowerFaults faults)
{
    SystemData_PowerStatus status;
    status.trans_on = BSP_Power_IsTransOn();
    status.fault_pc4 = faults.pc4;
    status.fault_pc0 = faults.pc0;
    status.latch_pc4 = (s_latch & BSP_NVM_FAULT_PC4) != 0U;
    status.latch_pc0 = (s_latch & BSP_NVM_FAULT_PC0) != 0U;
    SystemData_SetPowerStatus(status);
}

static void AppPower_LatchFaults(uint8_t bits)
{
    uint8_t updated = (uint8_t)(s_latch | bits);
    if (updated != s_latch)
    {
        s_latch = updated;
        BSP_NVM_WriteFaultLatch(s_latch);
    }
}

static void AppPower_LatchConfirmedFaults(BSP_PowerFaults faults)
{
    uint8_t bits = 0U;
    bool pc4_confirmed = AppPower_DebounceActive(faults.pc4 && BSP_Power_IsTransOn(),
                                                 &s_pc4_timing, &s_pc4_since_ms);
    bool pc0_confirmed = AppPower_DebounceActive(faults.pc0, &s_pc0_timing, &s_pc0_since_ms);
    if (pc4_confirmed) bits |= BSP_NVM_FAULT_PC4;
    if (pc0_confirmed) bits |= BSP_NVM_FAULT_PC0;
    if (bits == 0U) return;
    AppPower_LatchFaults(bits);
}

bool AppPower_Init(void)
{
    BSP_Power_Init();
    s_latch = BSP_NVM_ReadFaultLatch();
    AppPower_Update();
    return true;
}

void AppPower_Update(void)
{
    BSP_PowerFaults faults = BSP_Power_ReadFaults();
    AppPower_LatchConfirmedFaults(faults);
    /* Link loss is a WARNING cause only (decided 2026-10-15, reverses SRS-COM-045): it no
     * longer turns TRANS/VOUT off by itself. */
    if (BSP_Power_IsTransOn() && AppMode_Get() == APP_MODE_ERROR)
    {
        AppRetry_NoteShutdown();
        AppPower_TurnOffOutputs();
    }
    AppPower_PublishStatus(faults);
}

bool AppPower_SetTrans(bool enable)
{
    bool accepted;
    bool regulator_detected = false;
    bool was_on = BSP_Power_IsTransOn();
    BSP_PowerFaults faults;
    if (!enable) BSP_TPS55287_SetEnabled(false);
    accepted = enable ? (AppMode_Get() != APP_MODE_ERROR && BSP_Power_SetTrans(true))
                      : BSP_Power_SetTrans(false);
    /* This is the first chance to probe the TPS55287 after powering TRANS. A failed
     * probe is ERROR unless PC4 indicates the eFuse itself has interrupted TPS power. */
    if (accepted && enable && !was_on)
    {
        regulator_detected = BSP_TPS55287_Detect();
        if (!regulator_detected && !BSP_Power_ReadFaults().pc4)
        {
            AppMode_SetRegulatorFault();
            BSP_Power_SetTrans(false);
            accepted = false;
        }
    }
    /* Select and verify the internal 20 V feedback scale after each power-up,
     * before any voltage command can run against another ratio. */
    if (accepted && enable && !was_on && regulator_detected &&
        !BSP_TPS55287_SetInternalFeedback() && !BSP_Power_ReadFaults().pc4)
    {
        AppMode_SetRegulatorFault();
        BSP_Power_SetTrans(false);
        accepted = false;
    }
    /* If the eFuse FLT pin is active, its own hardware may have removed TPS power.
     * A failed probe or feedback write in that condition leaves TRANS commanded on. */
    /* A live PC4/PC0 fault does not reject this call. The 6 ms task confirms it
     * after 50 ms and latches it without a software output shutdown. */
    faults = BSP_Power_ReadFaults();
    AppPower_PublishStatus(faults);
    return accepted;
}

bool AppPower_EnsureRegulatorOn(void)
{
    return BSP_Power_IsTransOn() ? true : AppPower_SetTrans(true);
}

bool AppPower_ProbeRegulator(void)
{
    bool was_on = BSP_Power_IsTransOn();
    bool detected;
    if (!was_on && !BSP_Power_SetTrans(true)) return false;
    detected = BSP_TPS55287_Detect();
    if (!was_on) BSP_Power_SetTrans(false);
    return detected;
}

void AppPower_TurnOffOutputs(void)
{
    BSP_TPS55287_SetEnabled(false);
    BSP_Power_SetTrans(false);
    AppPower_PublishStatus(BSP_Power_ReadFaults());
}

bool AppPower_ClearFaultLatch(void)
{
    BSP_PowerFaults faults = BSP_Power_ReadFaults();
    if (faults.pc4 || faults.pc0)
    {
        AppPower_PublishStatus(faults);
        return false;
    }
    if (s_latch != 0U)
    {
        s_latch = 0U;
        BSP_NVM_WriteFaultLatch(0U);
    }
    AppPower_PublishStatus(faults);
    return true;
}
