#ifndef BSP_UART_H_
#define BSP_UART_H_

#include <stdbool.h>
#include <stdint.h>

#include "../../M3_Driver/Component/UART/uart.h"

/* Initialize only hardware USART0 (PC104), before enabling interrupts. */
bool BSP_UART0_Init(uint32_t cpu_hz, uint32_t baudrate);
void BSP_UART0_Deinit(void);
uart_dev_t *BSP_UART0_Device(void);

/* Configure this board's USART0 and console with the board clock and bauds. */
bool BSP_UART_InitBoard(void);

/* Initialize USART0 and the software console together. The BSP owns FIFO
 * storage, pin mapping, and interrupt vectors. */
bool BSP_UART_Init(uint32_t cpu_hz, uint32_t pc104_baud, uint32_t console_baud);
void BSP_UART_Deinit(void);
uart_dev_t *BSP_UART_PC104(void);
uart_dev_t *BSP_UART_Console(void);

/* Nonblocking byte I/O. False means invalid port, stopped port, TX full,
 * or RX empty. Writing UART0 starts its UDRE interrupt; the console uses
 * the Timer2 software UART interrupt. */
bool BSP_UART_Write(uart_number_t number, uint8_t value);
bool BSP_UART_Read(uart_number_t number, uint8_t *value);

#endif /* BSP_UART_H_ */
