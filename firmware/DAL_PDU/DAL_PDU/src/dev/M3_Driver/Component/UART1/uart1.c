#include "uart1.h"

#include <avr/io.h>

bool UART1_Init(uint32_t cpu_hz, uint32_t baud, uint8_t parity,
                uint8_t stop_bits)
{
    uint32_t divisor;
    uint32_t ubrr;
    uint8_t format = _BV(UCSZ11) | _BV(UCSZ10);

    if (cpu_hz == 0UL || baud == 0UL || baud > 1000000UL ||
        parity > 2U || (stop_bits != 1U && stop_bits != 2U))
    {
        return false;
    }
    divisor = baud * 8UL;
    ubrr = (cpu_hz + divisor / 2UL) / divisor;
    if (ubrr == 0UL || ubrr > 4096UL)
    {
        return false;
    }
    ubrr--;
    if (parity == 1U) /* odd */
    {
        format |= _BV(UPM11) | _BV(UPM10);
    }
    else if (parity == 2U) /* even */
    {
        format |= _BV(UPM11);
    }
    if (stop_bits == 2U)
    {
        format |= _BV(USBS1);
    }

    UCSR1B = 0U;
    UBRR1H = (uint8_t)(ubrr >> 8);
    UBRR1L = (uint8_t)ubrr;
    UCSR1A = _BV(U2X1);
    UCSR1C = format;
    UCSR1B = _BV(RXEN1) | _BV(TXEN1);
    return true;
}

void UART1_Deinit(void)
{
    UCSR1B = 0U;
}

void UART1_EnableRx(bool enable)
{
    if (enable) UCSR1B |= _BV(RXCIE1);
    else UCSR1B &= (uint8_t)~_BV(RXCIE1);
}

void UART1_EnableTxReady(bool enable)
{
    if (enable) UCSR1B |= _BV(UDRIE1);
    else UCSR1B &= (uint8_t)~_BV(UDRIE1);
}

void UART1_EnableTxComplete(bool enable)
{
    if (enable) UCSR1B |= _BV(TXCIE1);
    else UCSR1B &= (uint8_t)~_BV(TXCIE1);
}

bool UART1_GetByte(uint8_t *value)
{
    uint8_t status = UCSR1A;
    uint8_t data = UDR1;
    if (value == 0 || (status & (_BV(FE1) | _BV(DOR1) | _BV(UPE1))) != 0U)
    {
        return false;
    }
    *value = data;
    return true;
}

void UART1_PutByte(uint8_t value)
{
    UDR1 = value;
}
