#include "bsp_eeprom.h"

#include "../dal_pdu_board.h"
#include "../I2C/bsp_i2c.h"
#include "../../M3_Driver/Component/I2C/avr_i2c.h"
#include "../../M3_Driver/Devices/EEPROM24C04/eeprom24c04.h"

static bool s_initialized;

void BSP_EEPROM_Init(void)
{
    /* Drive high before making PA0 an output, including after a bootloader. */
    EEPROM_WP_PORT |= _BV(EEPROM_WP_PIN);
    EEPROM_WP_DDR |= _BV(EEPROM_WP_PIN);
    MCUCR &= (uint8_t)~_BV(SRE); /* XMEM otherwise owns PORTA. */
    s_initialized = true;
}

bool EEPROM24C04_BusRead(uint8_t device_address, uint8_t word_address,
                         uint8_t *data, uint8_t length)
{
    uint8_t index;
    bool ok;
    if (data == 0 || length == 0U) return false;
    ok = I2C_AVR_Start((uint8_t)(device_address << 1)) &&
         I2C_AVR_Write(word_address) &&
         I2C_AVR_Start((uint8_t)((device_address << 1) | 1U));
    for (index = 0U; ok && index < length; index++)
        ok = I2C_AVR_Read(&data[index], index + 1U < length);
    I2C_AVR_Stop();
    return ok;
}

bool EEPROM24C04_BusWritePage(uint8_t device_address, uint8_t word_address,
                              const uint8_t *data, uint8_t length)
{
    uint8_t index;
    bool ok;
    if (data == 0 || length == 0U || length > EEPROM24C04_PAGE_SIZE) return false;
    ok = I2C_AVR_Start((uint8_t)(device_address << 1)) &&
         I2C_AVR_Write(word_address);
    for (index = 0U; ok && index < length; index++)
        ok = I2C_AVR_Write(data[index]);
    I2C_AVR_Stop();
    return ok;
}

bool EEPROM24C04_BusReady(uint8_t device_address)
{
    return BSP_I2C_Probe(device_address);
}

bool BSP_EEPROM_Probe(void)
{
    return BSP_I2C_Probe(EEPROM24C04_BASE_ADDRESS);
}

bool BSP_EEPROM_Read(uint16_t address, uint8_t *data, uint16_t length)
{
    return EEPROM24C04_Read(address, data, length);
}

bool BSP_EEPROM_EraseAll(void)
{
    uint8_t blank[EEPROM24C04_PAGE_SIZE];
    uint16_t address;
    uint8_t i;
    for (i = 0U; i < EEPROM24C04_PAGE_SIZE; i++) blank[i] = 0xFFU;
    for (address = 0U; address < EEPROM24C04_SIZE_BYTES; address += EEPROM24C04_PAGE_SIZE)
    {
        if (!BSP_EEPROM_Write(address, blank, EEPROM24C04_PAGE_SIZE)) return false;
    }
    return true;
}

bool BSP_EEPROM_Write(uint16_t address, const uint8_t *data, uint16_t length)
{
    uint16_t offset;
    bool ok;
    uint8_t verify[EEPROM24C04_PAGE_SIZE];
    if (address > EEPROM24C04_SIZE_BYTES ||
        length > EEPROM24C04_SIZE_BYTES - address ||
        (length != 0U && data == 0)) return false;
    if (length == 0U) return true;
    if (!s_initialized) BSP_EEPROM_Init();
    EEPROM_WP_PORT &= (uint8_t)~_BV(EEPROM_WP_PIN);
    ok = EEPROM24C04_Write(address, data, length);
    EEPROM_WP_PORT |= _BV(EEPROM_WP_PIN);
    if (!ok) return false;

    /* A protected EEPROM ACKs writes without changing data. Check the result. */
    for (offset = 0U; offset < length; )
    {
        uint16_t remaining = (uint16_t)(length - offset);
        uint8_t count = (uint8_t)(remaining > EEPROM24C04_PAGE_SIZE ?
                                  EEPROM24C04_PAGE_SIZE : remaining);
        uint8_t index;
        if (!BSP_EEPROM_Read(address + offset, verify, count)) return false;
        for (index = 0U; index < count; index++)
            if (verify[index] != data[offset + index]) return false;
        offset += count;
    }
    return true;
}
