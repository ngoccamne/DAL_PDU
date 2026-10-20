#ifndef UART1_H_
#define UART1_H_

#include <stdbool.h>
#include <stdint.h>

/* USART1 register access for the Modbus RTU port. ISRs live in the BSP. */
bool UART1_Init(uint32_t cpu_hz, uint32_t baud, uint8_t parity,
                uint8_t stop_bits);
void UART1_Deinit(void);
void UART1_EnableRx(bool enable);
void UART1_EnableTxReady(bool enable);
void UART1_EnableTxComplete(bool enable);
bool UART1_GetByte(uint8_t *value);
void UART1_PutByte(uint8_t value);

#endif /* UART1_H_ */
