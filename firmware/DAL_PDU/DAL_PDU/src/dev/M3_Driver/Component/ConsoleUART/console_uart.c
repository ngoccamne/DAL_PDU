#include "console_uart.h"

#include <avr/io.h>

#include "../../../M2_BSP/dal_pdu_board.h"
#include "../SoftUART/soft_uart.h"

bool ConsoleUART_Init(uint32_t cpu_hz, uint32_t baud,
                      FIFO_t *rx_fifo, FIFO_t *tx_fifo)
{
    return SoftUART_Init(&SOFTUART_TX_DDR, &SOFTUART_TX_PORT,
                         _BV(SOFTUART_TX_PIN),
                         &SOFTUART_RX_DDR, &SOFTUART_RX_PORT,
                         &SOFTUART_RX_PIN_R, _BV(SOFTUART_RX_PIN),
                         cpu_hz, baud, rx_fifo, tx_fifo);
}

void ConsoleUART_Deinit(void)
{
    SoftUART_Deinit();
}

bool ConsoleUART_Write(uint8_t value)
{
    return SoftUART_Write(value);
}

bool ConsoleUART_Read(uint8_t *value)
{
    return SoftUART_Read(value);
}
