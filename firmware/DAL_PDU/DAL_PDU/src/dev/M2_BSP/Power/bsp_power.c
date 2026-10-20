#include "bsp_power.h"

#include <avr/io.h>
#include "../dal_pdu_board.h"
#include <util/delay.h>

#define TRANS_SETTLE_MS 20

BSP_PowerFaults BSP_Power_ReadFaults(void)
{
    BSP_PowerFaults faults;
    uint8_t pins = TRANS_FAULT_PIN_R;
    faults.pc4 = (pins & _BV(TRANS_FAULT_A_PIN)) == 0U;
    faults.pc0 = (pins & _BV(MCU_FAULT_PIN)) == 0U;
    return faults;
}

void BSP_Power_Init(void)
{
    TRANS_EN_PORT |= _BV(TRANS_EN_PIN); /* Disable before changing direction. */
    TRANS_EN_DDR |= _BV(TRANS_EN_PIN);
    TRANS_EN_DDR &= (uint8_t)~(_BV(TRANS_FAULT_A_PIN) | _BV(MCU_FAULT_PIN));
}

bool BSP_Power_SetTrans(bool enable)
{
    /* Live PC4/PC0 inputs are handled by the periodic application debounce. The
     * stored latch is checked by AppPower_SetTrans before this function is called. */
    if (enable)
    {
        if (!BSP_Power_IsTransOn())
        {
            TRANS_EN_PORT &= (uint8_t)~_BV(TRANS_EN_PIN);
            /* Allow the TRANS supply to settle before probing the TPS55287. */
            _delay_ms(TRANS_SETTLE_MS);
        }
    }
    else TRANS_EN_PORT |= _BV(TRANS_EN_PIN);
    return true;
}

bool BSP_Power_IsTransOn(void)
{
    return (TRANS_EN_PORT & _BV(TRANS_EN_PIN)) == 0U;
}
