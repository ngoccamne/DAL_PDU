#include <stdbool.h>
#include <stdint.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NVM/bsp_nvm.h"

/* Host stand-in for the clock checkpoint ring in internal EEPROM. */
static uint8_t fake_next;
static uint32_t fake_slot[BSP_NVM_CLOCK_SLOTS];
static bool fake_valid[BSP_NVM_CLOCK_SLOTS];
unsigned fake_clock_writes;

void FakeClockNvm_Reset(void)
{
    uint8_t i;
    fake_next = 0U;
    fake_clock_writes = 0U;
    for (i = 0U; i < BSP_NVM_CLOCK_SLOTS; i++) fake_valid[i] = false;
}

uint8_t BSP_NVM_ClockNext(void)
{
    return fake_next;
}

void BSP_NVM_SetClockNext(uint8_t next)
{
    fake_next = next;
}

bool BSP_NVM_ReadClockSlot(uint8_t slot, uint32_t *unix_s)
{
    if (unix_s == 0 || slot >= BSP_NVM_CLOCK_SLOTS || !fake_valid[slot]) return false;
    *unix_s = fake_slot[slot];
    return true;
}

void BSP_NVM_WriteClockSlot(uint8_t slot, uint32_t unix_s)
{
    if (slot >= BSP_NVM_CLOCK_SLOTS) return;
    fake_slot[slot] = unix_s;
    fake_valid[slot] = true;
    fake_clock_writes++;
}
