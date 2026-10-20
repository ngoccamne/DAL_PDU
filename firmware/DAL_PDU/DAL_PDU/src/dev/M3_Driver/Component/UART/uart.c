#include "uart.h"

#include <stddef.h>

bool uart_write(uart_dev_t *dev, uint8_t value)
{
    if (dev == NULL || dev->status != UART_STATUS_READY ||
        dev->tx_fifo == NULL || !uart_platform_can_write(dev) ||
        !FIFO_Push(dev->tx_fifo, value))
    {
        return false;
    }
    uart_platform_start_tx(dev);
    return true;
}

bool uart_read(uart_dev_t *dev, uint8_t *value)
{
    if (dev == NULL || value == NULL || dev->status != UART_STATUS_READY ||
        dev->rx_fifo == NULL)
    {
        return false;
    }
    return FIFO_Pop(dev->rx_fifo, value);
}

void uart_set_rx_callback(uart_dev_t *dev, uart_rx_callback_t callback,
                          void *user)
{
    if (dev != NULL)
    {
        dev->rx_callback = callback;
        dev->callback_user = user;
    }
}
