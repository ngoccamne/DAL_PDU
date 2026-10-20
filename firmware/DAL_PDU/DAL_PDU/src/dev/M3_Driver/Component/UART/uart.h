#ifndef UART_INTERFACE_H_
#define UART_INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>

#include "../../../M4_MiddleWare/FIFO/fifo.h"

/* Logical ports; an AVR or STM BSP maps each number to its implementation. */
typedef enum
{
    UART_NUMBER_UART0 = 0,
    UART_NUMBER_PC104 = UART_NUMBER_UART0,
    UART_NUMBER_CONSOLE = 1
} uart_number_t;

typedef enum
{
    UART_STATUS_STOPPED = 0,
    UART_STATUS_READY = 1
} uart_status_t;

struct uart_dev;
typedef struct uart_dev uart_dev_t;

/* Called from the RX ISR after a byte has been queued. Keep it short and
 * nonblocking; the callback can drain bytes with uart_read(). */
typedef void (*uart_rx_callback_t)(uart_dev_t *dev, void *user);
typedef void (*uart_tx_callback_t)(uart_dev_t *dev);

/* The BSP owns number, baudrate, status, FIFO pointers, and tx_callback.
 * Callers register the optional RX callback before global interrupts. */
struct uart_dev
{
    uart_number_t number;
    uint32_t baudrate;
    volatile uart_status_t status;
    FIFO_t *rx_fifo;
    FIFO_t *tx_fifo;
    uart_rx_callback_t rx_callback;
    void *callback_user;
    uart_tx_callback_t tx_callback;
};

/* Nonblocking. False means unavailable, TX full, or RX empty. */
bool uart_write(uart_dev_t *dev, uint8_t value);
bool uart_read(uart_dev_t *dev, uint8_t *value);

/* Configure before enabling global interrupts. */
void uart_set_rx_callback(uart_dev_t *dev, uart_rx_callback_t callback,
                          void *user);

/* Compile-time platform hooks implemented by the selected BSP. */
bool uart_platform_can_write(const uart_dev_t *dev);
void uart_platform_start_tx(uart_dev_t *dev);

#endif /* UART_INTERFACE_H_ */
