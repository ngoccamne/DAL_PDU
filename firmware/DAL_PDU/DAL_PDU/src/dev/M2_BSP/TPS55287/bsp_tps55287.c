#include "bsp_tps55287.h"

#include "../I2C/bsp_i2c.h"
#include "../Power/bsp_power.h"

static BSP_TPS55287_Status s_status;

bool TPS55287_BusRead(uint8_t address, uint8_t reg, uint8_t *value)
{
    return BSP_I2C_ReadRegister(address, reg, value);
}

bool TPS55287_BusWrite(uint8_t address, uint8_t reg, uint8_t value)
{
    return BSP_I2C_WriteRegister(address, reg, value);
}

BSP_TPS55287_Status BSP_TPS55287_GetStatus(void)
{
    return s_status;
}

void BSP_TPS55287_ResetStatus(void)
{
    BSP_TPS55287_Status blank = {0};
    s_status = blank;
}

bool BSP_TPS55287_Detect(void)
{
    uint8_t mode;
    BSP_TPS55287_ResetStatus(); /* Detection starts from a blank record. */
    if (BSP_I2C_ReadRegister(TPS55287_ADDR_MODE_HIGH, 0x06U, &mode))
    {
        s_status.address = TPS55287_ADDR_MODE_HIGH;
    }
    else if (BSP_I2C_ReadRegister(TPS55287_ADDR_MODE_LOW, 0x06U, &mode))
    {
        s_status.address = TPS55287_ADDR_MODE_LOW;
    }
    return s_status.address != 0U;
}

uint8_t BSP_TPS55287_Address(void)
{
    return s_status.address;
}

static bool EnsureDetected(void)
{
    return BSP_TPS55287_Address() != 0U || BSP_TPS55287_Detect();
}

/* A raw I2C command can change VOUT_FS while TRANS remains on. Correct it before
 * calculating a setpoint or enabling VOUT; turn OE off first if necessary. */
static bool PrepareInternalFeedback(const TPS55287_Device *device, bool *was_enabled,
                                    bool *changed)
{
    uint8_t mode;
    uint8_t feedback;
    *was_enabled = false;
    *changed = false;
    if (!BSP_I2C_ReadRegister(device->address, 0x06U, &mode) ||
        !BSP_I2C_ReadRegister(device->address, 0x04U, &feedback)) return false;
    if (feedback == TPS55287_INTERNAL_FEEDBACK_20V) return true;
    if ((mode & 0x80U) != 0U)
    {
        if (!TPS55287_SetEnabled(device, false)) return false;
        *was_enabled = true;
    }
    if (!TPS55287_SetInternalFeedback(device)) return false;
    *changed = true;
    return true;
}

bool BSP_TPS55287_ReadState(TPS55287_State *state)
{
    TPS55287_Device device;
    if (!EnsureDetected()) return false;
    device.address = s_status.address;
    if (!TPS55287_ReadState(&device, state))
    {
        s_status.state_valid = false;
        return false;
    }
    s_status.state_valid = true;
    s_status.mode = state->mode;
    s_status.feedback = state->feedback;
    s_status.status = state->status;
    s_status.reference_code = state->reference_code;
    return true;
}

bool BSP_TPS55287_SetVoltageMv(uint16_t millivolts)
{
    TPS55287_Device device;
    uint16_t actual_mv;
    bool was_enabled;
    bool changed;
    /* Reject an invalid request before touching an output that may be enabled. */
    if (millivolts < TPS55287_MIN_VOUT_MV || millivolts > TPS55287_MAX_VOUT_MV)
        return false;
    if (!EnsureDetected()) return false;
    device.address = s_status.address;
    if (!PrepareInternalFeedback(&device, &was_enabled, &changed)) return false;
    s_status.setpoint_valid = false;
    if (!TPS55287_SetVoltageMv(&device, millivolts)) return false;
    if (was_enabled && !TPS55287_SetEnabled(&device, true)) return false;
    s_status.setpoint_valid = true;
    s_status.setpoint_mv = millivolts;
    s_status.voltage_valid = TPS55287_GetVoltageMv(&device, &actual_mv);
    if (s_status.voltage_valid) s_status.voltage_mv = actual_mv;
    return true;
}

bool BSP_TPS55287_GetVoltageMv(uint16_t *millivolts)
{
    TPS55287_Device device;
    if (!EnsureDetected()) return false;
    device.address = s_status.address;
    s_status.voltage_valid = TPS55287_GetVoltageMv(&device, millivolts);
    if (s_status.voltage_valid) s_status.voltage_mv = *millivolts;
    return s_status.voltage_valid;
}

bool BSP_TPS55287_SetInternalFeedback(void)
{
    TPS55287_Device device;
    bool was_enabled;
    bool changed;
    if (!EnsureDetected()) return false;
    device.address = s_status.address;
    /* If feedback changed while VOUT was on, keep OE off until the reference
     * is programmed for the 20 V internal scale. */
    if (!PrepareInternalFeedback(&device, &was_enabled, &changed)) return false;
    if (changed) s_status.setpoint_valid = false;
    return true;
}

bool BSP_TPS55287_IsEnabled(bool *enabled)
{
    uint8_t mode;
    if (enabled == 0 || !EnsureDetected()) return false;
    if (!BSP_I2C_ReadRegister(s_status.address, 0x06U, &mode)) return false;
    *enabled = (mode & 0x80U) != 0U;
    s_status.mode = mode;
    return true;
}

bool BSP_TPS55287_SetEnabled(bool enabled)
{
    TPS55287_Device device;
    bool was_enabled;
    bool changed;
    /* Does not check the live PC4/PC0 pins here; see BSP_Power_SetTrans. TRANS must
     * still actually be on, since enabling VOUT with TRANS off is meaningless. */
    if (enabled && !BSP_Power_IsTransOn()) return false;
    if (!EnsureDetected()) return false;
    device.address = s_status.address;
    if (enabled && !PrepareInternalFeedback(&device, &was_enabled, &changed)) return false;
    /* A prior reference code may have been calculated for another feedback mode.
     * Reprogram a known setpoint, or leave OE off until `set` supplies one. */
    if (enabled && !s_status.setpoint_valid) return false;
    if (enabled && changed &&
        !TPS55287_SetVoltageMv(&device, s_status.setpoint_mv)) return false;
    if (!TPS55287_SetEnabled(&device, enabled)) return false;
    s_status.commanded_enable = enabled;
    if (s_status.state_valid)
    {
        if (enabled) s_status.mode |= 0x80U;
        else s_status.mode &= (uint8_t)~0x80U;
    }
    return true;
}
