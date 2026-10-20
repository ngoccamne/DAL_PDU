#include "bsp_pac1934.h"

#include "../dal_pdu_board.h"
#include "../I2C/bsp_i2c.h"

#include <util/delay.h>

static uint8_t s_address;

bool PAC1934_BusRead(uint8_t address, uint8_t reg, uint8_t *data, uint8_t length)
{
    return BSP_I2C_ReadBlock(address, reg, data, length);
}

bool PAC1934_BusCommand(uint8_t address, uint8_t command)
{
    return BSP_I2C_SendCommand(address, command);
}

bool BSP_PAC1934_Detect(void)
{
    uint8_t address;
    s_address = 0U;
    for (address = 0x10U; address <= 0x1FU; address++)
    {
        if (PAC1934_CheckId(address))
        {
            s_address = address;
            return true;
        }
    }
    return false;
}

uint8_t BSP_PAC1934_Address(void)
{
    return s_address;
}

bool BSP_PAC1934_ReadSnapshot(PAC1934_Sample *sample)
{
    if (sample == 0 || (s_address == 0U && !BSP_PAC1934_Detect())) return false;
    /* REFRESH_V latches averages without resetting the energy accumulators. */
    if (!PAC1934_Refresh(s_address))
    {
        s_address = 0U;
        return false;
    }
    _delay_ms(2);
    if (!PAC1934_ReadAll(s_address, PAC1934_SHUNT_MOHM, sample))
    {
        s_address = 0U;
        return false;
    }
    return true;
}
