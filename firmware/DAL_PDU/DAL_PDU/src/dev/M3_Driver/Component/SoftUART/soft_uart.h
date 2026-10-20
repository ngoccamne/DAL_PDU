#ifndef SOFT_UART_H_
#define SOFT_UART_H_

#include <stdbool.h>
#include <stdint.h>

#include "../../../M4_MiddleWare/FIFO/fifo.h"

/* One 8N1 software UART driven by Timer2 compare interrupts. Timer2 is owned
 * exclusively by this driver while active. TX and RX may use different ports.
 * Supported timing: 2400 to 19200 baud with a 12 MHz CPU clock. */
bool SoftUART_Init(volatile uint8_t *tx_ddr, volatile uint8_t *tx_port,
                   uint8_t tx_mask, volatile uint8_t *rx_ddr,
                   volatile uint8_t *rx_port, volatile uint8_t *rx_pin,
                   uint8_t rx_mask, uint32_t cpu_hz, uint32_t baud,
                   FIFO_t *rx_fifo, FIFO_t *tx_fifo);
void SoftUART_Deinit(void);
bool SoftUART_Write(uint8_t value);
bool SoftUART_Read(uint8_t *value);
bool SoftUART_TxIdle(void);
uint16_t SoftUART_RxDropped(void);
uint16_t SoftUART_RxFramingErrors(void);

/* Called by the board's Timer2 compare interrupt vector. */
bool SoftUART_OnTimerInterrupt(void);

#endif /* SOFT_UART_H_ */
