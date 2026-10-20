#include <stdio.h>
#include <stdint.h>

#include <avr/io.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Modbus/bsp_modbus.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M4_MiddleWare/ThirdParty/freemodbus/modbus/include/mbport.h"

volatile uint8_t UCSR1A, UCSR1B, UCSR1C, UBRR1H, UBRR1L, UDR1;
volatile uint8_t TCCR1A, TCCR1B, SREG, TIMSK, TIFR;
volatile uint16_t TCNT1, OCR1A;
volatile uint8_t DDRD, PORTD;

static unsigned failures;
static unsigned rx_calls;
static unsigned tx_calls;
static unsigned timer_calls;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

static BOOL on_rx(void) { rx_calls++; return TRUE; }
static BOOL on_tx(void) { tx_calls++; return TRUE; }
static BOOL on_timer(void) { timer_calls++; return TRUE; }
BOOL (*pxMBFrameCBByteReceived)(void) = on_rx;
BOOL (*pxMBFrameCBTransmitterEmpty)(void) = on_tx;
BOOL (*pxMBPortCBTimerExpired)(void) = on_timer;

void USART1_RX_vect(void);
void USART1_UDRE_vect(void);
void USART1_TX_vect(void);
void TIMER1_COMPA_vect(void);

int main(void)
{
    CHAR byte;
    CHECK(xMBPortSerialInit(1U, 9600UL, 8U, MB_PAR_NONE, 1U));
    CHECK((DDRD & _BV(PD5)) != 0U);
    CHECK((PORTD & _BV(PD5)) == 0U);
    CHECK(UBRR1H == 0U && UBRR1L == 155U);
    CHECK(xMBPortSerialInit(1U, 19200UL, 8U, MB_PAR_NONE, 1U));
    CHECK(UBRR1L == 77U);
    CHECK(xMBPortSerialInit(1U, 115200UL, 8U, MB_PAR_NONE, 1U));
    CHECK(UBRR1H == 0U && UBRR1L == 12U);
    CHECK(!xMBPortSerialInit(1U, 0UL, 8U, MB_PAR_NONE, 1U));
    CHECK(xMBPortSerialInit(1U, 9600UL, 8U, MB_PAR_NONE, 1U));
    CHECK(UBRR1L == 155U);
    vMBPortSerialEnable(TRUE, FALSE);
    CHECK((UCSR1B & _BV(RXCIE1)) != 0U);
    UDR1 = 'A';
    USART1_RX_vect();
    CHECK(rx_calls == 1U && xMBPortSerialGetByte(&byte) && byte == 'A');
    vMBPortSerialEnable(FALSE, TRUE);
    CHECK((PORTD & _BV(PD5)) != 0U);
    CHECK((UCSR1B & _BV(UDRIE1)) != 0U);
    CHECK((UCSR1B & _BV(RXCIE1)) == 0U);
    USART1_UDRE_vect();
    CHECK(tx_calls == 1U);
    vMBPortSerialEnable(TRUE, FALSE);
    CHECK((PORTD & _BV(PD5)) != 0U);
    CHECK((UCSR1B & _BV(TXCIE1)) != 0U);
    USART1_TX_vect();
    CHECK((PORTD & _BV(PD5)) == 0U);
    CHECK((UCSR1B & _BV(RXCIE1)) != 0U);
    CHECK(xMBPortTimersInit(80U));
    vMBPortTimersEnable();
    CHECK(OCR1A == 5999U);
    CHECK((TIMSK & _BV(OCIE1A)) != 0U);
    TIMER1_COMPA_vect();
    CHECK(timer_calls == 1U && (TIMSK & _BV(OCIE1A)) == 0U);
    vMBPortClose();
    return failures ? 1 : 0;
}
