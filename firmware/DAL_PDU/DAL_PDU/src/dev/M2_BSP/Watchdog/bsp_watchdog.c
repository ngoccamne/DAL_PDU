#include "bsp_watchdog.h"

#include <avr/io.h>
#include <stdint.h>
#include <util/atomic.h>

#include "../dal_pdu_board.h"

static volatile uint8_t s_inhibited;
static volatile uint8_t s_initialized;

bool BSP_Watchdog_Init(void)
{
    /* Set DONE low before enabling its output driver. Do not pull WAKE up. */
    EXT_WDT_FEED_PORT &= (uint8_t)~_BV(EXT_WDT_FEED_PIN);
    EXT_WDT_WAKE_PORT &= (uint8_t)~_BV(EXT_WDT_WAKE_PIN);
    EXT_WDT_WAKE_DDR &= (uint8_t)~_BV(EXT_WDT_WAKE_PIN);
    EXT_WDT_FEED_DDR |= _BV(EXT_WDT_FEED_PIN);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_inhibited = 0U;
        s_initialized = 1U;
    }
    return true;
}

void BSP_Watchdog_SetDone(bool high)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (s_initialized && !s_inhibited && high)
        {
            EXT_WDT_FEED_PORT |= _BV(EXT_WDT_FEED_PIN);
        }
        else
        {
            EXT_WDT_FEED_PORT &= (uint8_t)~_BV(EXT_WDT_FEED_PIN);
        }
    }
}

void BSP_Watchdog_Inhibit(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_inhibited = 1U;
        EXT_WDT_FEED_PORT &= (uint8_t)~_BV(EXT_WDT_FEED_PIN);
    }
}
