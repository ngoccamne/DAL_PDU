#include "bsp_modbus.h"
#include "port.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include "../dal_pdu_board.h"
#include "../../M3_Driver/Component/UART1/uart1.h"
#include "../../M4_MiddleWare/ThirdParty/freemodbus/modbus/include/mbport.h"

static volatile uint8_t s_rx_byte;
static volatile bool s_rx_valid;
static volatile bool s_tx_draining;
static volatile bool s_rx_after_tx;
static uint16_t s_timer_compare;
static volatile uint8_t s_critical_depth;
static uint8_t s_saved_sreg;

bool BSP_Modbus_InitPins(void)
{
    RS485_DIR_PORT &= (uint8_t)~_BV(RS485_DIR_PIN); /* receive */
    RS485_DIR_DDR |= _BV(RS485_DIR_PIN);
    RS485_TX_PORT |= _BV(RS485_TX_PIN);
    RS485_TX_DDR |= _BV(RS485_TX_PIN);
    RS485_RX_DDR &= (uint8_t)~_BV(RS485_RX_PIN);
    RS485_RX_PORT &= (uint8_t)~_BV(RS485_RX_PIN);
    return true;
}

void BSP_Modbus_DeinitPins(void)
{
    RS485_DIR_PORT &= (uint8_t)~_BV(RS485_DIR_PIN);
}

void BSP_Modbus_EnterCritical(void)
{
    uint8_t state = SREG;
    cli();
    if (s_critical_depth++ == 0U) s_saved_sreg = state;
}

void BSP_Modbus_ExitCritical(void)
{
    if (s_critical_depth != 0U && --s_critical_depth == 0U)
    {
        SREG = s_saved_sreg;
    }
}

BOOL xMBPortSerialInit(UCHAR port, ULONG baud, UCHAR bits,
                       eMBParity parity, UCHAR stops)
{
    if (port != 1U || bits != 8U || !BSP_Modbus_InitPins()) return FALSE;
    s_rx_valid = false;
    s_tx_draining = false;
    s_rx_after_tx = false;
    return UART1_Init(F_CPU, baud, (uint8_t)parity, stops) ? TRUE : FALSE;
}

void vMBPortSerialEnable(BOOL rx_enable, BOOL tx_enable)
{
    if (tx_enable)
    {
        UART1_EnableRx(false);
        s_tx_draining = false;
        s_rx_after_tx = false;
        RS485_DIR_PORT |= _BV(RS485_DIR_PIN);
        UCSR1A |= _BV(TXC1); /* discard stale completion */
        UART1_EnableTxComplete(false);
        UART1_EnableTxReady(true);
    }
    else
    {
        UART1_EnableTxReady(false);
        if ((RS485_DIR_PORT & _BV(RS485_DIR_PIN)) != 0U)
        {
            /* Last byte is still on the wire; release DE only at TXC. */
            s_tx_draining = true;
            s_rx_after_tx = (rx_enable != FALSE);
            UART1_EnableTxComplete(true);
        }
        else
        {
            UART1_EnableRx(rx_enable != FALSE);
        }
    }
}

BOOL xMBPortSerialGetByte(CHAR *value)
{
    if (value == 0 || !s_rx_valid) return FALSE;
    *value = (CHAR)s_rx_byte;
    s_rx_valid = false;
    return TRUE;
}

BOOL xMBPortSerialPutByte(CHAR value)
{
    UART1_PutByte((uint8_t)value);
    return TRUE;
}

void vMBPortClose(void)
{
    UART1_Deinit();
    BSP_Modbus_DeinitPins();
}

void xMBPortSerialClose(void)
{
    vMBPortClose();
}

BOOL xMBPortTimersInit(USHORT timeout_50us)
{
    uint32_t ticks = (uint32_t)timeout_50us * (F_CPU / 8UL / 20000UL);
    if (ticks == 0UL || ticks > 65536UL) return FALSE;
    s_timer_compare = (uint16_t)(ticks - 1UL);
    TCCR1A = 0U;
    TCCR1B = _BV(WGM12); /* stopped, CTC */
    TIMSK &= (uint8_t)~_BV(OCIE1A);
    return TRUE;
}

void vMBPortTimersEnable(void)
{
    TCCR1B = _BV(WGM12);
    TCNT1 = 0U;
    OCR1A = s_timer_compare;
    TIFR = _BV(OCF1A);
    TIMSK |= _BV(OCIE1A);
    TCCR1B = _BV(WGM12) | _BV(CS11); /* F_CPU/8 */
}

void vMBPortTimersDisable(void)
{
    TIMSK &= (uint8_t)~_BV(OCIE1A);
    TCCR1B = _BV(WGM12);
}

void xMBPortTimersClose(void)
{
    vMBPortTimersDisable();
}

void vMBPortTimersDelay(USHORT ms)
{
    (void)ms; /* RTU response delay is configured as zero. */
}

ISR(USART1_RX_vect)
{
    uint8_t byte;
    if (UART1_GetByte(&byte))
    {
        s_rx_byte = byte;
        s_rx_valid = true;
        if (pxMBFrameCBByteReceived != 0) (void)pxMBFrameCBByteReceived();
    }
}

ISR(USART1_UDRE_vect)
{
    if (pxMBFrameCBTransmitterEmpty != 0)
        (void)pxMBFrameCBTransmitterEmpty();
    else UART1_EnableTxReady(false);
}

ISR(USART1_TX_vect)
{
    UART1_EnableTxComplete(false);
    if (s_tx_draining)
    {
        s_tx_draining = false;
        RS485_DIR_PORT &= (uint8_t)~_BV(RS485_DIR_PIN);
        UART1_EnableRx(s_rx_after_tx);
    }
}

ISR(TIMER1_COMPA_vect)
{
    vMBPortTimersDisable();
    if (pxMBPortCBTimerExpired != 0) (void)pxMBPortCBTimerExpired();
}
