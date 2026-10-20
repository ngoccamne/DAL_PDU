#include "bsp_i2c.h"

#include <avr/io.h>

#include "../dal_pdu_board.h"
#include "../../M3_Driver/Component/I2C/avr_i2c.h"

static bool s_bus_fault;
static uint8_t s_fault_address;

/* A timeout means the bus is busy or stuck, so the TWI is reset and the fault stays set until a
 * transaction completes. A NACK is an absent device, which is normal, so it is not a fault. */
static bool BSP_I2C_Finish(bool ok, uint8_t address)
{
    if (I2C_AVR_TimedOut())
    {
        I2C_AVR_Recover();
        s_bus_fault = true;
        s_fault_address = address;
        return false;
    }
    I2C_AVR_Stop();
    if (ok)
    {
        s_bus_fault = false;
        s_fault_address = 0U;
    }
    return ok;
}

bool BSP_I2C_BusFault(void)
{
    return s_bus_fault;
}

uint8_t BSP_I2C_FaultAddress(void)
{
    return s_fault_address;
}

bool BSP_I2C_Init(void)
{
    TWI_SCL_DDR &= (uint8_t)~_BV(TWI_SCL_PIN);
    TWI_SDA_DDR &= (uint8_t)~_BV(TWI_SDA_PIN);
    TWI_SCL_PORT |= _BV(TWI_SCL_PIN);
    TWI_SDA_PORT |= _BV(TWI_SDA_PIN);
    return I2C_AVR_Init(F_CPU, TWI_SCL_FREQ_HZ);
}

bool BSP_I2C_Probe(uint8_t address)
{
    bool ok;
    I2C_AVR_ClearTimeout();
    if (address > 0x7FU) return false;
    ok = I2C_AVR_Start((uint8_t)(address << 1));
    return BSP_I2C_Finish(ok, address);
}

bool BSP_I2C_ReadByte(uint8_t address, uint8_t *value)
{
    bool ok;
    I2C_AVR_ClearTimeout();
    if (address > 0x7FU || value == 0) return false;
    ok = I2C_AVR_Start((uint8_t)((address << 1) | 1U)) && I2C_AVR_Read(value, false);
    return BSP_I2C_Finish(ok, address);
}

bool BSP_I2C_ReadRegister(uint8_t address, uint8_t reg, uint8_t *value)
{
    bool ok;
    I2C_AVR_ClearTimeout();
    if (address > 0x7FU || value == 0) return false;
    ok = I2C_AVR_Start((uint8_t)(address << 1)) && I2C_AVR_Write(reg) &&
         I2C_AVR_Start((uint8_t)((address << 1) | 1U)) && I2C_AVR_Read(value, false);
    return BSP_I2C_Finish(ok, address);
}

bool BSP_I2C_WriteRegister(uint8_t address, uint8_t reg, uint8_t value)
{
    bool ok;
    I2C_AVR_ClearTimeout();
    if (address > 0x7FU) return false;
    ok = I2C_AVR_Start((uint8_t)(address << 1)) && I2C_AVR_Write(reg) && I2C_AVR_Write(value);
    return BSP_I2C_Finish(ok, address);
}

bool BSP_I2C_ReadBlock(uint8_t address, uint8_t reg, uint8_t *data, uint8_t length)
{
    uint8_t index;
    bool ok;
    I2C_AVR_ClearTimeout();
    if (address > 0x7FU || data == 0 || length == 0U) return false;
    ok = I2C_AVR_Start((uint8_t)(address << 1)) && I2C_AVR_Write(reg) &&
         I2C_AVR_Start((uint8_t)((address << 1) | 1U));
    for (index = 0U; ok && index < length; index++)
        ok = I2C_AVR_Read(&data[index], index + 1U < length);
    return BSP_I2C_Finish(ok, address);
}

bool BSP_I2C_SendCommand(uint8_t address, uint8_t command)
{
    bool ok;
    I2C_AVR_ClearTimeout();
    if (address > 0x7FU) return false;
    ok = I2C_AVR_Start((uint8_t)(address << 1)) && I2C_AVR_Write(command);
    return BSP_I2C_Finish(ok, address);
}
