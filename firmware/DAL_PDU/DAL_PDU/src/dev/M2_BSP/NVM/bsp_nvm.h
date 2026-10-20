#ifndef BSP_NVM_H_
#define BSP_NVM_H_

#include <stdbool.h>
#include <stdint.h>

/* Fault latch bits stored in the ATmega128A internal EEPROM. */
#define BSP_NVM_FAULT_PC4 0x01U
#define BSP_NVM_FAULT_PC0 0x02U
#define BSP_NVM_FAULT_ALL 0x03U

/* Returns the latched fault bits. A blank EEPROM means none. A damaged record
 * returns both bits so that power stays off until the latch is cleared. */
uint8_t BSP_NVM_ReadFaultLatch(void);

/* Writes the latch record. The complement and magic are written last, so an
 * interrupted write reads back as damaged, which latches both faults. */
void BSP_NVM_WriteFaultLatch(uint8_t bits);

/* Limit settings stored in the internal EEPROM. */
typedef struct
{
    int16_t temp_trip_tenths_c;
    uint16_t vout_max_ma;
    uint16_t trans5v_max_ma;
} BSP_NVM_Limits;

/* Returns false when the record is blank or damaged, and leaves *limits unchanged. */
bool BSP_NVM_ReadLimits(BSP_NVM_Limits *limits);

/* Limit ERROR flag. Set on a trip in NORMAL and cleared by a successful retry. Blank
 * means clear. Any other value means set, so a damaged byte fails safe. */
bool BSP_NVM_ReadLimitError(void);
void BSP_NVM_WriteLimitError(bool set);
void BSP_NVM_WriteLimits(const BSP_NVM_Limits *limits);

/* Boot record in internal EEPROM, rewritten once per boot. */
typedef struct
{
    uint16_t boot_count;
    uint8_t cause;
    uint8_t source;
} BSP_NVM_BootRecord;

/* Returns false when the record is blank or damaged. */
bool BSP_NVM_ReadBootRecord(BSP_NVM_BootRecord *record);
void BSP_NVM_WriteBootRecord(const BSP_NVM_BootRecord *record);

/* Boot history: a ring of the last BSP_NVM_HISTORY_SLOTS boots, plus the slot to write next. */
#define BSP_NVM_HISTORY_SLOTS 10U
uint8_t BSP_NVM_BootHistoryNext(void);
void BSP_NVM_SetBootHistoryNext(uint8_t next);
bool BSP_NVM_ReadBootHistory(uint8_t slot, BSP_NVM_BootRecord *record);
void BSP_NVM_WriteBootHistory(uint8_t slot, const BSP_NVM_BootRecord *record);

/* Version 2 adds automatic retry settings. Version 1 records remain readable. */
#define BSP_NVM_CONFIG_VERSION 2U
typedef struct
{
    uint8_t slave_address;
    uint8_t baud_code;
    uint8_t comms_timeout_s;
    uint8_t retry_enable;
    uint8_t retry_time_s;
} BSP_NVM_Config;

/* Returns false when the record is blank, damaged, or of another version. */
bool BSP_NVM_ReadConfig(BSP_NVM_Config *config);
void BSP_NVM_WriteConfig(const BSP_NVM_Config *config);

/* Clock checkpoint: a ring of BSP_NVM_CLOCK_SLOTS stored times. The slot written last is the
 * latest, by write order, not by value, so a clock set backwards is still restored correctly. */
#define BSP_NVM_CLOCK_SLOTS 8U
uint8_t BSP_NVM_ClockNext(void);
void BSP_NVM_SetClockNext(uint8_t next);
/* Returns false when the slot is blank or damaged. */
bool BSP_NVM_ReadClockSlot(uint8_t slot, uint32_t *unix_s);
void BSP_NVM_WriteClockSlot(uint8_t slot, uint32_t unix_s);

/* Blanks application EEPROM records but preserves bootloader bytes 0xF80..0xFC1.
 * Application records then read back as blank/default. Used by persist_reset. */
void BSP_NVM_EraseAll(void);

#endif /* BSP_NVM_H_ */
