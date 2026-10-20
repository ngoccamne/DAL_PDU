#include "pac1934.h"

#define PAC1934_REG_VBUS1_AVG   0x0FU
#define PAC1934_REG_VSENSE1_AVG 0x13U
#define PAC1934_REG_VPOWER1     0x17U
#define PAC1934_REG_CHANNEL_DIS_ACT 0x22U
#define PAC1934_REG_NEG_PWR_ACT     0x23U

static bool ReadWord(uint8_t address, uint8_t reg, uint16_t *value)
{
    uint8_t bytes[2];
    if (!PAC1934_BusRead(address, reg, bytes, sizeof(bytes))) return false;
    *value = (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
    return true;
}

bool PAC1934_CheckId(uint8_t address)
{
    uint8_t product_id;
    return PAC1934_BusRead(address, PAC1934_REG_PRODUCT_ID, &product_id, 1U) &&
           product_id == PAC1934_PRODUCT_ID;
}

bool PAC1934_Refresh(uint8_t address)
{
    return PAC1934_BusCommand(address, PAC1934_CMD_REFRESH_V);
}

bool PAC1934_ReadAll(uint8_t address, uint16_t shunt_mohm, PAC1934_Sample *sample)
{
    PAC1934_Sample result;
    uint8_t channel;
    uint16_t bus_raw;
    uint16_t sense_raw;
    uint8_t power_bytes[4];
    uint32_t power_raw;
    uint8_t channel_disabled;
    uint8_t negative_power;
    if (sample == 0 || shunt_mohm == 0U) return false;
    if (!PAC1934_BusRead(address, PAC1934_REG_CHANNEL_DIS_ACT,
                         &channel_disabled, 1U) ||
        !PAC1934_BusRead(address, PAC1934_REG_NEG_PWR_ACT,
                         &negative_power, 1U) ||
        channel_disabled != 0U || negative_power != 0U)
        return false;
    for (channel = 0U; channel < PAC1934_CHANNEL_COUNT; channel++)
    {
        if (!ReadWord(address, (uint8_t)(PAC1934_REG_VBUS1_AVG + channel), &bus_raw) ||
            !ReadWord(address, (uint8_t)(PAC1934_REG_VSENSE1_AVG + channel), &sense_raw) ||
            !PAC1934_BusRead(address, (uint8_t)(PAC1934_REG_VPOWER1 + channel),
                             power_bytes, sizeof(power_bytes)))
            return false;
        /* Unipolar defaults: VBUS full scale 32 V, VSENSE full scale 100 mV. */
        result.voltage_mv[channel] = (uint16_t)(((uint32_t)bus_raw * 32000UL + 32768UL) / 65536UL);
        result.current_ma[channel] = (uint32_t)(((uint64_t)sense_raw * 100000UL +
            (uint32_t)shunt_mohm * 32768UL) / ((uint32_t)shunt_mohm * 65536UL));
        power_raw = (((uint32_t)power_bytes[0] << 24) |
                     ((uint32_t)power_bytes[1] << 16) |
                     ((uint32_t)power_bytes[2] << 8) | power_bytes[3]) >> 4;
        /* VPOWER is a 28-bit fraction of (32 V * 100 mV / Rshunt). */
        result.power_mw[channel] = (uint32_t)(((uint64_t)power_raw * 3200000ULL +
            ((uint64_t)shunt_mohm << 27)) / ((uint64_t)shunt_mohm << 28));
    }
    *sample = result;
    return true;
}
