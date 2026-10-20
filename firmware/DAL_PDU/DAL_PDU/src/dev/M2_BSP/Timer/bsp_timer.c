#include "bsp_timer.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include "../dal_pdu_board.h"
#include "../../M1_Sys/Scheduler/scheduler.h"

/* Timer0 is 8-bit. With the /64 prescaler there are F_CPU / 64000 counts per ms. At 16 MHz that is
 * exactly 250. At 12 MHz it is 187.5, so the integer 187 gives a 1.0027 ms tick, 0.27 % slow. */
#if (F_CPU / 64000UL) == 0UL || (F_CPU / 64000UL) > 256UL
#error Timer0 cannot generate a 1 ms tick with the board clock and /64 prescaler
#endif

bool BSP_Timer_Init1ms(void)
{
    TIMSK &= (uint8_t)~_BV(OCIE0);
    TCCR0 = 0U;
    TCNT0 = 0U;
    OCR0 = (uint8_t)((F_CPU / 64000UL) - 1UL);
    TIFR = _BV(OCF0);
    TCCR0 = _BV(WGM01) | _BV(CS01) | _BV(CS00);
    TIMSK |= _BV(OCIE0);
    return true;
}

void BSP_Timer_Deinit(void)
{
    TIMSK &= (uint8_t)~_BV(OCIE0);
    TCCR0 = 0U;
}

/* Keep interrupts enabled while updating scheduler counters so Timer2 can
 * sample the software UART without waiting for this lower-rate tick. */
ISR(TIMER0_COMP_vect, ISR_NOBLOCK)
{
    SCH_RunSystemTickTimer();
}
