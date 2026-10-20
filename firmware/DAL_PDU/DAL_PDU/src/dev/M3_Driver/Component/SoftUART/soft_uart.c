#include "soft_uart.h"

#include <avr/io.h>
#include <stddef.h>
#include <util/atomic.h>

/* Three samples per bit reduce Timer2 ISR load at console baud rates. */
#define SOFT_UART_SAMPLES_PER_BIT 3U

typedef enum
{
    TX_IDLE,
    TX_DATA,
    TX_STOP,
    TX_FINISH
} TxState;

typedef enum
{
    RX_IDLE,
    RX_START,
    RX_DATA,
    RX_STOP
} RxState;

static volatile uint8_t *s_tx_port;
static volatile uint8_t *s_rx_pin;
static uint8_t s_tx_mask;
static uint8_t s_rx_mask;
static FIFO_t *s_rx_fifo;
static FIFO_t *s_tx_fifo;
static volatile bool s_active;
static volatile TxState s_tx_state;
static volatile RxState s_rx_state;
static uint8_t s_tx_ticks;
static uint8_t s_rx_ticks;
static uint8_t s_tx_byte;
static uint8_t s_rx_byte;
static uint8_t s_bit_index;
static uint8_t s_rx_bit_index;
static volatile uint16_t s_rx_dropped;
static volatile uint16_t s_rx_framing_errors;

bool SoftUART_Init(volatile uint8_t *tx_ddr, volatile uint8_t *tx_port,
                   uint8_t tx_mask, volatile uint8_t *rx_ddr,
                   volatile uint8_t *rx_port, volatile uint8_t *rx_pin,
                   uint8_t rx_mask, uint32_t cpu_hz, uint32_t baud,
                   FIFO_t *rx_fifo, FIFO_t *tx_fifo)
{
    uint32_t divisor;
    uint32_t ticks;
    uint8_t clock_select = _BV(CS21);

    if (tx_ddr == NULL || tx_port == NULL || tx_mask == 0U ||
        rx_ddr == NULL || rx_port == NULL || rx_pin == NULL || rx_mask == 0U ||
        (tx_ddr == rx_ddr && (tx_mask & rx_mask) != 0U) ||
        rx_fifo == NULL || tx_fifo == NULL || rx_fifo == tx_fifo ||
        rx_fifo->buffer == NULL || tx_fifo->buffer == NULL ||
        rx_fifo->capacity == 0U || tx_fifo->capacity == 0U ||
        baud < 2400U || baud > 19200U || cpu_hz == 0U)
    {
        return false;
    }

    divisor = baud * 8UL * SOFT_UART_SAMPLES_PER_BIT;
    ticks = (cpu_hz + divisor / 2UL) / divisor;
    if (ticks > 256U)
    {
        divisor = baud * 32UL * SOFT_UART_SAMPLES_PER_BIT;
        ticks = (cpu_hz + divisor / 2UL) / divisor;
        clock_select = _BV(CS21) | _BV(CS20);
    }
    if (ticks < 16U || ticks > 256U)
    {
        return false;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        TIMSK &= (uint8_t)~_BV(OCIE2);
        TCCR2 = 0U;
        s_active = false;
        s_tx_port = tx_port;
        s_rx_pin = rx_pin;
        s_tx_mask = tx_mask;
        s_rx_mask = rx_mask;
        s_rx_fifo = rx_fifo;
        s_tx_fifo = tx_fifo;
        s_tx_state = TX_IDLE;
        s_rx_state = RX_IDLE;
        s_tx_ticks = 0U;
        s_rx_ticks = 0U;
        s_tx_byte = 0U;
        s_rx_byte = 0U;
        s_bit_index = 0U;
        s_rx_bit_index = 0U;
        s_rx_dropped = 0U;
        s_rx_framing_errors = 0U;

        *tx_port |= tx_mask;
        *tx_ddr |= tx_mask;
        *rx_ddr &= (uint8_t)~rx_mask;
        *rx_port |= rx_mask; /* Weak pull-up keeps an unplugged RX idle high. */

        OCR2 = (uint8_t)(ticks - 1U);
        TCNT2 = 0U;
        TIFR = _BV(OCF2);
        s_active = true;
        TCCR2 = _BV(WGM21) | clock_select; /* CTC, clock /8 or /32. */
        TIMSK |= _BV(OCIE2);
    }
    return true;
}

void SoftUART_Deinit(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        TIMSK &= (uint8_t)~_BV(OCIE2);
        TCCR2 = 0U;
        s_active = false;
        if (s_tx_port != NULL)
        {
            *s_tx_port |= s_tx_mask;
        }
        s_rx_fifo = NULL;
        s_tx_fifo = NULL;
    }
}

bool SoftUART_Write(uint8_t value)
{
    bool written = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (s_active && s_tx_fifo != NULL)
        {
            written = FIFO_Push(s_tx_fifo, value);
        }
    }
    return written;
}

bool SoftUART_Read(uint8_t *value)
{
    bool read = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (s_active && s_rx_fifo != NULL)
        {
            read = FIFO_Pop(s_rx_fifo, value);
        }
    }
    return read;
}

bool SoftUART_TxIdle(void)
{
    bool idle = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        idle = s_active && s_tx_state == TX_IDLE && FIFO_Count(s_tx_fifo) == 0U;
    }
    return idle;
}

uint16_t SoftUART_RxDropped(void)
{
    uint16_t value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        value = s_rx_dropped;
    }
    return value;
}

uint16_t SoftUART_RxFramingErrors(void)
{
    uint16_t value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        value = s_rx_framing_errors;
    }
    return value;
}

static void SoftUART_TxTick(void)
{
    if (s_tx_state == TX_IDLE)
    {
        if (FIFO_Pop(s_tx_fifo, &s_tx_byte))
        {
            *s_tx_port &= (uint8_t)~s_tx_mask;
            s_tx_state = TX_DATA;
            s_tx_ticks = SOFT_UART_SAMPLES_PER_BIT;
            s_bit_index = 0U;
        }
        return;
    }

    if (--s_tx_ticks != 0U)
    {
        return;
    }

    if (s_tx_state == TX_DATA)
    {
        if ((s_tx_byte & 1U) != 0U)
        {
            *s_tx_port |= s_tx_mask;
        }
        else
        {
            *s_tx_port &= (uint8_t)~s_tx_mask;
        }
        s_tx_byte >>= 1;
        s_bit_index++;
        s_tx_state = (s_bit_index == 8U) ? TX_STOP : TX_DATA;
        s_tx_ticks = SOFT_UART_SAMPLES_PER_BIT;
    }
    else if (s_tx_state == TX_STOP)
    {
        *s_tx_port |= s_tx_mask;
        s_tx_state = TX_FINISH;
        s_tx_ticks = SOFT_UART_SAMPLES_PER_BIT;
    }
    else
    {
        s_tx_state = TX_IDLE;
    }
}

static bool SoftUART_RxTick(void)
{
    bool high = ((*s_rx_pin & s_rx_mask) != 0U);

    if (s_rx_state == RX_IDLE)
    {
        if (!high)
        {
            s_rx_state = RX_START;
            s_rx_ticks = SOFT_UART_SAMPLES_PER_BIT / 2U;
        }
        return false;
    }

    if (--s_rx_ticks != 0U)
    {
        return false;
    }

    if (s_rx_state == RX_START)
    {
        if (high)
        {
            s_rx_state = RX_IDLE;
        }
        else
        {
            s_rx_state = RX_DATA;
            s_rx_ticks = SOFT_UART_SAMPLES_PER_BIT;
            s_rx_byte = 0U;
            s_rx_bit_index = 0U;
        }
    }
    else if (s_rx_state == RX_DATA)
    {
        if (high)
        {
            s_rx_byte |= (uint8_t)(1U << s_rx_bit_index);
        }
        s_rx_bit_index++;
        s_rx_state = (s_rx_bit_index == 8U) ? RX_STOP : RX_DATA;
        s_rx_ticks = SOFT_UART_SAMPLES_PER_BIT;
    }
    else
    {
        if (high)
        {
            if (!FIFO_Push(s_rx_fifo, s_rx_byte))
            {
                s_rx_dropped++;
            }
            else
            {
                s_rx_state = RX_IDLE;
                return true;
            }
        }
        else
        {
            s_rx_framing_errors++;
        }
        s_rx_state = RX_IDLE;
    }
    return false;
}

bool SoftUART_OnTimerInterrupt(void)
{
    if (s_active)
    {
        bool received = SoftUART_RxTick();
        SoftUART_TxTick();
        return received;
    }
    return false;
}
