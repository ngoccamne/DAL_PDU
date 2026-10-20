#include "bsp_led.h"

#include "../dal_pdu_board.h"
#include "../../M3_Driver/Component/DIO/do.h"

static do_t s_heartbeat_led =
{
    .port = DO_PORT_A,
    .pin = LED_BLINK_PIN,
    .bStatus = false
};

bool BSP_LED_Init(void)
{
    /* XMEM takes over PORTA; a bootloader may leave it enabled. */
    MCUCR &= (uint8_t)~_BV(SRE);
    s_heartbeat_led.bStatus = false;
    return do_init(&s_heartbeat_led);
}

void BSP_LED_Set(bool on)
{
    if (on)
    {
        do_set(&s_heartbeat_led);
    }
    else
    {
        do_reset(&s_heartbeat_led);
    }
}

void BSP_LED_Toggle(void)
{
    do_toggle(&s_heartbeat_led);
}
