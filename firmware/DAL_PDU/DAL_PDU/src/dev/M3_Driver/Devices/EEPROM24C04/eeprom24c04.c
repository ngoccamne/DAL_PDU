#include "eeprom24c04.h"

#define EEPROM24C04_ACK_POLL_LIMIT 80U

static bool ValidRange(uint16_t address, const void *data, uint16_t length)
{
    return address <= EEPROM24C04_SIZE_BYTES &&
           length <= EEPROM24C04_SIZE_BYTES - address &&
           (length == 0U || data != 0);
}

static uint8_t DeviceAddress(uint16_t address)
{
    return (uint8_t)(EEPROM24C04_BASE_ADDRESS | (address >> 8));
}

bool EEPROM24C04_Read(uint16_t address, uint8_t *data, uint16_t length)
{
    if (!ValidRange(address, data, length)) return false;
    while (length != 0U)
    {
        uint8_t chunk = (uint8_t)(EEPROM24C04_PAGE_SIZE -
                                  (address & (EEPROM24C04_PAGE_SIZE - 1U)));
        if (chunk > length) chunk = (uint8_t)length;
        if (!EEPROM24C04_BusRead(DeviceAddress(address), (uint8_t)address,
                                 data, chunk)) return false;
        address += chunk;
        data += chunk;
        length -= chunk;
    }
    return true;
}

bool EEPROM24C04_Write(uint16_t address, const uint8_t *data, uint16_t length)
{
    if (!ValidRange(address, data, length)) return false;
    while (length != 0U)
    {
        uint8_t chunk = (uint8_t)(EEPROM24C04_PAGE_SIZE -
                                  (address & (EEPROM24C04_PAGE_SIZE - 1U)));
        uint8_t attempts;
        uint8_t device_address = DeviceAddress(address);
        if (chunk > length) chunk = (uint8_t)length;
        if (!EEPROM24C04_BusWritePage(device_address, (uint8_t)address,
                                      data, chunk)) return false;
        /* The device NACKs while its internal write cycle is in progress. */
        for (attempts = 0U; attempts < EEPROM24C04_ACK_POLL_LIMIT; attempts++)
        {
            if (EEPROM24C04_BusReady(device_address)) break;
        }
        if (attempts == EEPROM24C04_ACK_POLL_LIMIT) return false;
        address += chunk;
        data += chunk;
        length -= chunk;
    }
    return true;
}
