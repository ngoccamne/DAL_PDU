#ifndef CONSOLE_UART_H_
#define CONSOLE_UART_H_

#include <stdbool.h>
#include <stdint.h>

#include "../../../M4_MiddleWare/FIFO/fifo.h"

/* P2 console: PE3 TX, PE4 RX. These pins are GPIO, not a hardware USART. */
bool ConsoleUART_Init(uint32_t cpu_hz, uint32_t baud,
                      FIFO_t *rx_fifo, FIFO_t *tx_fifo);
void ConsoleUART_Deinit(void);
bool ConsoleUART_Write(uint8_t value);
bool ConsoleUART_Read(uint8_t *value);

#endif /* CONSOLE_UART_H_ */
