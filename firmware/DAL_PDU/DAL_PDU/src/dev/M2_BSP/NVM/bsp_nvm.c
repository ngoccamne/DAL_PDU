#include "bsp_nvm.h"

#include <avr/eeprom.h>
#include <avr/io.h>

#define NVM_FAULT_ADDRESS   0U
#define NVM_FAULT_MAGIC     0xA5U
#define NVM_FAULT_BLANK     0xFFU
#define NVM_FAULT_ALL       BSP_NVM_FAULT_ALL

uint8_t BSP_NVM_ReadFaultLatch(void)
{
    uint8_t magic = eeprom_read_byte((const uint8_t *)(NVM_FAULT_ADDRESS));
    uint8_t bits;
    uint8_t complement;

    if (magic == NVM_FAULT_BLANK) return 0U;
    bits = eeprom_read_byte((const uint8_t *)(NVM_FAULT_ADDRESS + 1U));
    complement = eeprom_read_byte((const uint8_t *)(NVM_FAULT_ADDRESS + 2U));
    if (magic != NVM_FAULT_MAGIC || (uint8_t)(bits ^ complement) != 0xFFU ||
        (bits & (uint8_t)~NVM_FAULT_ALL) != 0U)
    {
        return NVM_FAULT_ALL;
    }
    return bits;
}

void BSP_NVM_WriteFaultLatch(uint8_t bits)
{
    bits &= NVM_FAULT_ALL;
    eeprom_update_byte((uint8_t *)(NVM_FAULT_ADDRESS + 1U), bits);
    eeprom_update_byte((uint8_t *)(NVM_FAULT_ADDRESS + 2U), (uint8_t)~bits);
    eeprom_update_byte((uint8_t *)(NVM_FAULT_ADDRESS), NVM_FAULT_MAGIC);
}

#define NVM_LIMITS_ADDRESS  16U
#define NVM_LIMITS_MAGIC    0xB7U
#define NVM_LIMITS_BYTES    6U

static uint8_t LimitsChecksum(const uint8_t *data)
{
    uint8_t sum = 0U;
    uint8_t i;
    for (i = 0U; i < NVM_LIMITS_BYTES; i++) sum = (uint8_t)(sum + data[i]);
    return sum;
}

bool BSP_NVM_ReadLimits(BSP_NVM_Limits *limits)
{
    uint8_t data[NVM_LIMITS_BYTES];
    uint8_t i;
    if (limits == 0) return false;
    if (eeprom_read_byte((const uint8_t *)NVM_LIMITS_ADDRESS) != NVM_LIMITS_MAGIC) return false;
    for (i = 0U; i < NVM_LIMITS_BYTES; i++)
    {
        data[i] = eeprom_read_byte((const uint8_t *)(NVM_LIMITS_ADDRESS + 1U + i));
    }
    if (eeprom_read_byte((const uint8_t *)(NVM_LIMITS_ADDRESS + 1U + NVM_LIMITS_BYTES)) != LimitsChecksum(data))
    {
        return false;
    }
    limits->temp_trip_tenths_c = (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
    limits->vout_max_ma = (uint16_t)((uint16_t)data[2] | ((uint16_t)data[3] << 8));
    limits->trans5v_max_ma = (uint16_t)((uint16_t)data[4] | ((uint16_t)data[5] << 8));
    return true;
}

void BSP_NVM_WriteLimits(const BSP_NVM_Limits *limits)
{
    uint8_t data[NVM_LIMITS_BYTES];
    uint8_t i;
    uint16_t temp = (uint16_t)limits->temp_trip_tenths_c;
    data[0] = (uint8_t)temp;
    data[1] = (uint8_t)(temp >> 8);
    data[2] = (uint8_t)limits->vout_max_ma;
    data[3] = (uint8_t)(limits->vout_max_ma >> 8);
    data[4] = (uint8_t)limits->trans5v_max_ma;
    data[5] = (uint8_t)(limits->trans5v_max_ma >> 8);
    for (i = 0U; i < NVM_LIMITS_BYTES; i++)
    {
        eeprom_update_byte((uint8_t *)(NVM_LIMITS_ADDRESS + 1U + i), data[i]);
    }
    eeprom_update_byte((uint8_t *)(NVM_LIMITS_ADDRESS + 1U + NVM_LIMITS_BYTES), LimitsChecksum(data));
    eeprom_update_byte((uint8_t *)NVM_LIMITS_ADDRESS, NVM_LIMITS_MAGIC);
}

#define NVM_LIMIT_ERROR_ADDRESS 24U
#define NVM_LIMIT_ERROR_SET 0x5AU
#define NVM_LIMIT_ERROR_CLEAR 0xFFU

bool BSP_NVM_ReadLimitError(void)
{
    return eeprom_read_byte((const uint8_t *)NVM_LIMIT_ERROR_ADDRESS) != NVM_LIMIT_ERROR_CLEAR;
}

void BSP_NVM_WriteLimitError(bool set)
{
    eeprom_update_byte((uint8_t *)NVM_LIMIT_ERROR_ADDRESS, set ? NVM_LIMIT_ERROR_SET : NVM_LIMIT_ERROR_CLEAR);
}

#define NVM_BOOT_ADDRESS 32U
#define NVM_BOOT_MAGIC 0xB1U
#define NVM_BOOT_DATA_BYTES 4U
#define NVM_HISTORY_INDEX_ADDRESS 40U
#define NVM_HISTORY_BASE 48U
#define NVM_HISTORY_STRIDE 5U /* four data bytes and a checksum */

static uint8_t BootChecksum(const uint8_t *data)
{
    uint8_t sum = 0U;
    uint8_t i;
    for (i = 0U; i < NVM_BOOT_DATA_BYTES; i++) sum = (uint8_t)(sum + data[i]);
    return sum;
}

static void BootWrite(uint16_t address, const BSP_NVM_BootRecord *record)
{
    uint8_t data[NVM_BOOT_DATA_BYTES];
    uint8_t i;
    data[0] = (uint8_t)record->boot_count;
    data[1] = (uint8_t)(record->boot_count >> 8);
    data[2] = record->cause;
    data[3] = record->source;
    for (i = 0U; i < NVM_BOOT_DATA_BYTES; i++)
    {
        eeprom_update_byte((uint8_t *)(address + i), data[i]);
    }
    eeprom_update_byte((uint8_t *)(address + NVM_BOOT_DATA_BYTES), BootChecksum(data));
}

static bool BootRead(uint16_t address, BSP_NVM_BootRecord *record)
{
    uint8_t data[NVM_BOOT_DATA_BYTES];
    uint8_t i;
    for (i = 0U; i < NVM_BOOT_DATA_BYTES; i++)
    {
        data[i] = eeprom_read_byte((const uint8_t *)(address + i));
    }
    if (eeprom_read_byte((const uint8_t *)(address + NVM_BOOT_DATA_BYTES)) != BootChecksum(data))
    {
        return false;
    }
    record->boot_count = (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
    record->cause = data[2];
    record->source = data[3];
    return true;
}

bool BSP_NVM_ReadBootRecord(BSP_NVM_BootRecord *record)
{
    if (record == 0) return false;
    if (eeprom_read_byte((const uint8_t *)NVM_BOOT_ADDRESS) != NVM_BOOT_MAGIC) return false;
    return BootRead(NVM_BOOT_ADDRESS + 1U, record);
}

void BSP_NVM_WriteBootRecord(const BSP_NVM_BootRecord *record)
{
    BootWrite(NVM_BOOT_ADDRESS + 1U, record);
    eeprom_update_byte((uint8_t *)NVM_BOOT_ADDRESS, NVM_BOOT_MAGIC);
}

uint8_t BSP_NVM_BootHistoryNext(void)
{
    uint8_t next = eeprom_read_byte((const uint8_t *)NVM_HISTORY_INDEX_ADDRESS);
    return (next < BSP_NVM_HISTORY_SLOTS) ? next : 0U;
}

void BSP_NVM_SetBootHistoryNext(uint8_t next)
{
    eeprom_update_byte((uint8_t *)NVM_HISTORY_INDEX_ADDRESS, next);
}

bool BSP_NVM_ReadBootHistory(uint8_t slot, BSP_NVM_BootRecord *record)
{
    if (record == 0 || slot >= BSP_NVM_HISTORY_SLOTS) return false;
    return BootRead((uint16_t)(NVM_HISTORY_BASE + slot * NVM_HISTORY_STRIDE), record);
}

void BSP_NVM_WriteBootHistory(uint8_t slot, const BSP_NVM_BootRecord *record)
{
    if (slot >= BSP_NVM_HISTORY_SLOTS) return;
    BootWrite((uint16_t)(NVM_HISTORY_BASE + slot * NVM_HISTORY_STRIDE), record);
}

#define NVM_CONFIG_ADDRESS 100U
#define NVM_CONFIG_MAGIC 0xC3U
#define NVM_CONFIG_DATA_BYTES 6U /* version, address, baud, timeout, retry enable/time */

static uint8_t ConfigChecksum(const uint8_t *data, uint8_t length)
{
    uint8_t sum = 0U;
    uint8_t i;
    for (i = 0U; i < length; i++) sum = (uint8_t)(sum + data[i]);
    return sum;
}

bool BSP_NVM_ReadConfig(BSP_NVM_Config *config)
{
    uint8_t data[NVM_CONFIG_DATA_BYTES];
    uint8_t i;
    uint8_t length;
    if (config == 0) return false;
    if (eeprom_read_byte((const uint8_t *)NVM_CONFIG_ADDRESS) != NVM_CONFIG_MAGIC) return false;
    data[0] = eeprom_read_byte((const uint8_t *)(NVM_CONFIG_ADDRESS + 1U));
    if (data[0] == 1U) length = 4U;
    else if (data[0] == BSP_NVM_CONFIG_VERSION) length = NVM_CONFIG_DATA_BYTES;
    else return false;
    for (i = 1U; i < length; i++)
    {
        data[i] = eeprom_read_byte((const uint8_t *)(NVM_CONFIG_ADDRESS + 1U + i));
    }
    if (eeprom_read_byte((const uint8_t *)(NVM_CONFIG_ADDRESS + 1U + length)) !=
        ConfigChecksum(data, length))
    {
        return false;
    }
    config->slave_address = data[1];
    config->baud_code = data[2];
    config->comms_timeout_s = data[3];
    config->retry_enable = (length == 4U) ? 1U : data[4];
    config->retry_time_s = (length == 4U) ? 5U : data[5];
    return true;
}

void BSP_NVM_WriteConfig(const BSP_NVM_Config *config)
{
    uint8_t data[NVM_CONFIG_DATA_BYTES];
    uint8_t i;
    data[0] = BSP_NVM_CONFIG_VERSION;
    data[1] = config->slave_address;
    data[2] = config->baud_code;
    data[3] = config->comms_timeout_s;
    data[4] = config->retry_enable;
    data[5] = config->retry_time_s;
    for (i = 0U; i < NVM_CONFIG_DATA_BYTES; i++)
    {
        eeprom_update_byte((uint8_t *)(NVM_CONFIG_ADDRESS + 1U + i), data[i]);
    }
    eeprom_update_byte((uint8_t *)(NVM_CONFIG_ADDRESS + 1U + NVM_CONFIG_DATA_BYTES),
                       ConfigChecksum(data, NVM_CONFIG_DATA_BYTES));
    /* The magic is written last, so an interrupted write reads back as blank. */
    eeprom_update_byte((uint8_t *)NVM_CONFIG_ADDRESS, NVM_CONFIG_MAGIC);
}

#define NVM_CLOCK_INDEX_ADDRESS 110U
#define NVM_CLOCK_BASE 112U
#define NVM_CLOCK_STRIDE 5U /* four time bytes and a checksum */
#define NVM_CLOCK_CHECK_KEY 0xA5U

static uint8_t ClockChecksum(const uint8_t *data)
{
    uint8_t sum = 0U;
    uint8_t i;
    for (i = 0U; i < 4U; i++) sum = (uint8_t)(sum + data[i]);
    /* A blank slot (0xFF) never matches, because its checksum is not 0xFF. */
    return (uint8_t)(sum ^ NVM_CLOCK_CHECK_KEY);
}

uint8_t BSP_NVM_ClockNext(void)
{
    uint8_t next = eeprom_read_byte((const uint8_t *)NVM_CLOCK_INDEX_ADDRESS);
    return (next < BSP_NVM_CLOCK_SLOTS) ? next : 0U;
}

void BSP_NVM_SetClockNext(uint8_t next)
{
    eeprom_update_byte((uint8_t *)NVM_CLOCK_INDEX_ADDRESS, next);
}

bool BSP_NVM_ReadClockSlot(uint8_t slot, uint32_t *unix_s)
{
    uint8_t data[4];
    uint16_t address;
    uint8_t i;
    if (unix_s == 0 || slot >= BSP_NVM_CLOCK_SLOTS) return false;
    address = (uint16_t)(NVM_CLOCK_BASE + slot * NVM_CLOCK_STRIDE);
    for (i = 0U; i < 4U; i++) data[i] = eeprom_read_byte((const uint8_t *)(address + i));
    if (eeprom_read_byte((const uint8_t *)(address + 4U)) != ClockChecksum(data)) return false;
    *unix_s = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    return true;
}

void BSP_NVM_WriteClockSlot(uint8_t slot, uint32_t unix_s)
{
    uint8_t data[4];
    uint16_t address;
    uint8_t i;
    if (slot >= BSP_NVM_CLOCK_SLOTS) return;
    data[0] = (uint8_t)unix_s;
    data[1] = (uint8_t)(unix_s >> 8);
    data[2] = (uint8_t)(unix_s >> 16);
    data[3] = (uint8_t)(unix_s >> 24);
    address = (uint16_t)(NVM_CLOCK_BASE + slot * NVM_CLOCK_STRIDE);
    for (i = 0U; i < 4U; i++) eeprom_update_byte((uint8_t *)(address + i), data[i]);
    eeprom_update_byte((uint8_t *)(address + 4U), ClockChecksum(data));
}

void BSP_NVM_EraseAll(void)
{
    uint16_t address;
    /* eeprom_update_byte skips the write when the byte already reads 0xFF, so most of
     * this loop is a cheap read on a chip that has not been heavily written to. */
    /* 0xF80..0xFC1 belongs to the AVR bootloader journal and request. */
    for (address = 0U; address <= E2END; address++)
    {
        if (address < 0x0F80U || address > 0x0FC1U)
            eeprom_update_byte((uint8_t *)address, 0xFFU);
    }
}
