#include "../do.h"

#include <avr/io.h>
#include <stddef.h>
#include <util/atomic.h>


static bool do_resolve(const do_t *me, volatile uint8_t **ddr,
                       volatile uint8_t **port, uint8_t *mask)
{
    if (me == NULL || me->pin > 7U ||
        (me->port == DO_PORT_G && me->pin > 4U))
    {
        return false;
    }

    switch (me->port)
    {
        case DO_PORT_A: *ddr = &DDRA; *port = &PORTA; break;
        case DO_PORT_B: *ddr = &DDRB; *port = &PORTB; break;
        case DO_PORT_C: *ddr = &DDRC; *port = &PORTC; break;
        case DO_PORT_D: *ddr = &DDRD; *port = &PORTD; break;
        case DO_PORT_E: *ddr = &DDRE; *port = &PORTE; break;
        case DO_PORT_F: *ddr = &DDRF; *port = &PORTF; break;
        case DO_PORT_G: *ddr = &DDRG; *port = &PORTG; break;
        default: return false;
    }

    *mask = (uint8_t)(1U << me->pin);
    return true;
}

bool do_init(do_t *me)
{
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    uint8_t mask;

    if (!do_resolve(me, &ddr, &port, &mask))
    {
        return false;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (me->bStatus)
        {
            *port |= mask;
        }
        else
        {
            *port &= (uint8_t)~mask;
        }
        *ddr |= mask;
    }
    return true;
}

void do_set(do_t *me)
{
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    uint8_t mask;

    if (!do_resolve(me, &ddr, &port, &mask))
    {
        return;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        *port |= mask;
        me->bStatus = true;
    }
}

void do_reset(do_t *me)
{
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    uint8_t mask;

    if (!do_resolve(me, &ddr, &port, &mask))
    {
        return;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        *port &= (uint8_t)~mask;
        me->bStatus = false;
    }
}

void do_toggle(do_t *me)
{
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    uint8_t mask;

    if (!do_resolve(me, &ddr, &port, &mask))
    {
        return;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        *port ^= mask;
        me->bStatus = (*port & mask) != 0U;
    }
}
