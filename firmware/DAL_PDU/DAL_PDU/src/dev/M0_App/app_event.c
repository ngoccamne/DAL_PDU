#include "app_event.h"

#include "app_clock.h"
#include "app_mode.h"
#include "../M2_BSP/EEPROM/bsp_eeprom.h"

/* 24C04 layout: a 4-byte header at 0, then APP_EVENT_SLOTS records of 12 bytes at 16.
 * The header magic changed with the record size, so a ring written by an older build is
 * ignored rather than misread. */
#define HEADER_ADDRESS 0U
#define HEADER_BYTES 4U
#define HEADER_MAGIC 0x5FU
#define RECORD_BASE 16U
#define RECORD_BYTES 12U
#define RECORD_MAGIC 0xA5U

static uint16_t Load16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t Load32(const uint8_t *data)
{
    return (uint32_t)Load16(data) | ((uint32_t)Load16(&data[2]) << 16);
}

bool AppEvent_Log(uint8_t severity, uint8_t code, int16_t value)
{
    uint8_t header[HEADER_BYTES];
    uint8_t record[RECORD_BYTES];
    uint8_t slot = 0U;
    uint16_t seq = 0U;
    uint16_t value_bits = (uint16_t)value;
    uint32_t time_s = AppClock_Now();

    if (!BSP_EEPROM_Read(HEADER_ADDRESS, header, HEADER_BYTES)) return false;
    if (header[0] == HEADER_MAGIC)
    {
        slot = header[1] < APP_EVENT_SLOTS ? header[1] : 0U;
        seq = Load16(&header[2]);
    }
    seq++;

    record[0] = RECORD_MAGIC;
    record[1] = severity;
    record[2] = code;
    record[3] = (uint8_t)((uint8_t)AppMode_Get() | (uint8_t)(AppClock_Source() << 4));
    record[4] = (uint8_t)value_bits;
    record[5] = (uint8_t)(value_bits >> 8);
    record[6] = (uint8_t)seq;
    record[7] = (uint8_t)(seq >> 8);
    record[8] = (uint8_t)time_s;
    record[9] = (uint8_t)(time_s >> 8);
    record[10] = (uint8_t)(time_s >> 16);
    record[11] = (uint8_t)(time_s >> 24);
    if (!BSP_EEPROM_Write((uint16_t)(RECORD_BASE + slot * RECORD_BYTES), record, RECORD_BYTES)) return false;

    header[0] = HEADER_MAGIC;
    header[1] = (uint8_t)((slot + 1U) % APP_EVENT_SLOTS);
    header[2] = (uint8_t)seq;
    header[3] = (uint8_t)(seq >> 8);
    return BSP_EEPROM_Write(HEADER_ADDRESS, header, HEADER_BYTES);
}

bool AppEvent_Read(uint8_t slot, AppEvent *event)
{
    uint8_t record[RECORD_BYTES];
    if (event == 0 || slot >= APP_EVENT_SLOTS) return false;
    if (!BSP_EEPROM_Read((uint16_t)(RECORD_BASE + slot * RECORD_BYTES), record, RECORD_BYTES)) return false;
    if (record[0] != RECORD_MAGIC) return false;
    event->severity = record[1];
    event->code = record[2];
    event->mode = record[3] & 0x0FU;
    event->clock_source = (uint8_t)(record[3] >> 4);
    event->value = (int16_t)Load16(&record[4]);
    event->seq = Load16(&record[6]);
    event->time_s = Load32(&record[8]);
    return true;
}

/* Sequence numbers wrap at 65536, so "newer" is judged by the distance from the current best. */
bool AppEvent_Find(uint16_t seq, AppEvent *event)
{
    AppEvent candidate;
    bool found = false;
    uint8_t slot;
    if (event == 0) return false;
    for (slot = 0U; slot < APP_EVENT_SLOTS; slot++)
    {
        if (!AppEvent_Read(slot, &candidate)) continue;
        if (seq != 0U)
        {
            if (candidate.seq == seq)
            {
                *event = candidate;
                return true;
            }
            continue;
        }
        if (!found || (uint16_t)(candidate.seq - event->seq) < 0x8000U)
        {
            *event = candidate;
            found = true;
        }
    }
    return found;
}
