#include "tps55287.h"

#define REG_REF_LSB 0x00U
#define REG_REF_MSB 0x01U
#define REG_VOUT_FS 0x04U
#define REG_MODE    0x06U
#define REG_STATUS  0x07U
#define MODE_OE     0x80U

static bool ValidDevice(const TPS55287_Device *device)
{
    return device != 0 && (device->address == TPS55287_ADDR_MODE_HIGH ||
                           device->address == TPS55287_ADDR_MODE_LOW);
}

static bool ReadReference(const TPS55287_Device *device, uint16_t *code)
{
    uint8_t low;
    uint8_t high;
    if (!TPS55287_BusRead(device->address, REG_REF_LSB, &low) ||
        !TPS55287_BusRead(device->address, REG_REF_MSB, &high)) return false;
    *code = (uint16_t)(((uint16_t)(high & 0x07U) << 8) | low);
    return true;
}

bool TPS55287_ReadState(const TPS55287_Device *device, TPS55287_State *state)
{
    if (!ValidDevice(device) || state == 0) return false;
    if (!TPS55287_BusRead(device->address, REG_MODE, &state->mode) ||
        !TPS55287_BusRead(device->address, REG_VOUT_FS, &state->feedback) ||
        !ReadReference(device, &state->reference_code) ||
        !TPS55287_BusRead(device->address, REG_STATUS, &state->status)) return false;
    return true;
}

bool TPS55287_SetEnabled(const TPS55287_Device *device, bool enabled)
{
    uint8_t mode;
    if (!ValidDevice(device) || !TPS55287_BusRead(device->address, REG_MODE, &mode)) return false;
    mode = enabled ? (uint8_t)(mode | MODE_OE) : (uint8_t)(mode & (uint8_t)~MODE_OE);
    return TPS55287_BusWrite(device->address, REG_MODE, mode);
}

bool TPS55287_SetVoltageMv(const TPS55287_Device *device, uint16_t millivolts)
{
    uint8_t feedback;
    uint16_t code;
    if (!ValidDevice(device) || !TPS55287_BusRead(device->address, REG_VOUT_FS, &feedback))
        return false;
    if (feedback != TPS55287_INTERNAL_FEEDBACK_20V ||
        millivolts < TPS55287_MIN_VOUT_MV || millivolts > TPS55287_MAX_VOUT_MV)
        return false;
    /* TI's 20 V internal scale is 800 mV at code 0, with 10 mV per code.
     * Round requests to the nearest supported step (20 V is code 0x780). */
    code = (uint16_t)((millivolts - TPS55287_MIN_VOUT_MV + 5U) / 10U);
    /* TI requires the low reference byte to be written before the high byte. */
    return TPS55287_BusWrite(device->address, REG_REF_LSB, (uint8_t)code) &&
           TPS55287_BusWrite(device->address, REG_REF_MSB, (uint8_t)(code >> 8));
}

bool TPS55287_GetVoltageMv(const TPS55287_Device *device, uint16_t *millivolts)
{
    uint8_t feedback;
    uint16_t code;
    if (!ValidDevice(device) || millivolts == 0 ||
        !TPS55287_BusRead(device->address, REG_VOUT_FS, &feedback) ||
        !ReadReference(device, &code)) return false;
    if (feedback != TPS55287_INTERNAL_FEEDBACK_20V) return false;
    *millivolts = (uint16_t)(TPS55287_MIN_VOUT_MV + code * 10U);
    return true;
}

bool TPS55287_SetInternalFeedback(const TPS55287_Device *device)
{
    uint8_t feedback;
    if (!ValidDevice(device)) return false;
    if (!TPS55287_BusWrite(device->address, REG_VOUT_FS,
                           TPS55287_INTERNAL_FEEDBACK_20V)) return false;
    if (!TPS55287_BusRead(device->address, REG_VOUT_FS, &feedback)) return false;
    return feedback == TPS55287_INTERNAL_FEEDBACK_20V;
}
