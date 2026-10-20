#ifndef UART0_H_
#define UART0_H_

#include <stdbool.h>
#include <stdint.h>

#include "../../../M4_MiddleWare/FIFO/fifo.h"
#include "../UART/uart.h"

/* ATmega128A USART0 on PE0 (RXD0) and PE1 (TXD0), 8N1, double speed.
 * Initialize the caller-owned FIFOs before calling this function. */
bool UART0_Init(uint32_t cpu_hz, uint32_t baud, FIFO_t *rx_fifo, FIFO_t *tx_fifo);
void UART0_Deinit(void);
bool UART0_Read(uint8_t *value);
uint16_t UART0_RxDropped(void);
uint16_t UART0_RxErrors(void);

/* Called by the board's USART0 interrupt vectors. */
bool UART0_OnRxInterrupt(void);
void UART0_TxStart(void);
void UART0_TxReadyCallback(uart_dev_t *dev);

#endif /* UART0_H_ */
