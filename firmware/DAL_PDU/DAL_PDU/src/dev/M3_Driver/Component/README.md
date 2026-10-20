# UART components

`UART0/` drives the ATmega128A hardware USART0 on PE0/PE1. The board design
assigns it to the PC104 RS-232 channel; the local debug connector shares that
route and must not be used at the same time. `ConsoleUART/` binds the generic
`SoftUART/` driver to the P2 console pins: PE3 TX and PE4 RX. Those console pins
are GPIO, not a hardware USART.

Application code uses `M3_Driver/Component/UART/uart.h`. To start only USART0, call
`BSP_UART0_Init(12000000UL, 19200UL)` before enabling global interrupts and
obtain its BSP-owned device with `BSP_UART0_Device()`. `BSP_UART_Init` remains
available when both USART0 and the software console are needed. Each
`uart_dev_t` holds a logical
UART number, baud rate, status, RX/TX FIFO pointers, and one optional RX
callback. The AVR BSP owns the four FIFO buffers in `M2_BSP/UART/bsp_uart.c`:

```c
BSP_UART0_Init(12000000UL, 19200UL); /* Before global interrupts. */
uart_dev_t *uart0 = BSP_UART0_Device();
uart_write(uart0, 'H');
```

`uart_write` pushes a byte to `dev->tx_fifo`, then the AVR BSP enables USART0's
UDRE interrupt. The BSP ISR calls the device's TX callback; that callback pops
the next byte into `UDR0`, or disables UDRE when the FIFO is empty. Console TX
uses its continuously running Timer2 interrupt instead. Both APIs are
nonblocking; a failed write means the TX FIFO is full. Call
`uart_set_rx_callback` before enabling global interrupts; the
callback runs inside the receive ISR after a byte enters the RX FIFO and may
drain it with `uart_read`. A future STM BSP can implement the same
`uart_platform_can_write` and `uart_platform_start_tx` hooks and logical UART
numbers without changing callers.
AVR interrupt vectors are defined only in `bsp_uart.c` and delegate to
component handlers. The software UART owns Timer2 and
provides one active channel at a time; it cannot also operate the current
RS-485 GPIO link concurrently. Its timer tick samples at three times the baud
rate, so verify actual RX/TX timing and interrupt load on the board before
using higher baud rates. `SoftUART_TxIdle` can be used before releasing an
RS-485 direction pin if the generic driver is used for that link later.

The PC104 RS-232 path has a documented hardware wiring blocker (`IF-001`).
Neither UART driver is started by `main.c` yet.
