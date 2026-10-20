#include "bsp_uart.h"

#include <avr/interrupt.h>
#include <stddef.h>

#include "../dal_pdu_board.h"
#include "../../M3_Driver/Component/UART0/uart0.h"
#include "../../M3_Driver/Component/ConsoleUART/console_uart.h"
#include "../../M3_Driver/Component/SoftUART/soft_uart.h"
#include "../../M4_MiddleWare/FIFO/fifo.h"

#ifndef BSP_UART_PC104_RX_CAPACITY
#define BSP_UART_PC104_RX_CAPACITY 128U
#endif
#ifndef BSP_UART_PC104_TX_CAPACITY
#define BSP_UART_PC104_TX_CAPACITY 128U
#endif
#ifndef BSP_UART_CONSOLE_RX_CAPACITY
#define BSP_UART_CONSOLE_RX_CAPACITY 64U
#endif
#ifndef BSP_UART_CONSOLE_TX_CAPACITY
#define BSP_UART_CONSOLE_TX_CAPACITY 128U /* Holds the version reply with its build ID and the prompt. */
#endif

/* Board-owned storage; component drivers receive FIFO pointers only. */
static uint8_t s_pc104_rx_storage[BSP_UART_PC104_RX_CAPACITY];
static uint8_t s_pc104_tx_storage[BSP_UART_PC104_TX_CAPACITY];
static uint8_t s_console_rx_storage[BSP_UART_CONSOLE_RX_CAPACITY];
static uint8_t s_console_tx_storage[BSP_UART_CONSOLE_TX_CAPACITY];
static FIFO_t s_pc104_rx_fifo;
static FIFO_t s_pc104_tx_fifo;
static FIFO_t s_console_rx_fifo;
static FIFO_t s_console_tx_fifo;
static bool s_uart0_initialized;
static bool s_console_initialized;

static void BSP_UART0_ConfigurePins(void)
{
    /* Set the idle TX level before enabling its output driver. */
    UART_XTP_TX_PORT |= _BV(UART_XTP_TX_PIN);
    UART_XTP_TX_DDR |= _BV(UART_XTP_TX_PIN);
    UART_XTP_RX_DDR &= (uint8_t)~_BV(UART_XTP_RX_PIN);
    UART_XTP_RX_PORT &= (uint8_t)~_BV(UART_XTP_RX_PIN);
}

static uart_dev_t s_uart0_device =
{
    .number = UART_NUMBER_UART0,
    .status = UART_STATUS_STOPPED,
    .rx_fifo = &s_pc104_rx_fifo,
    .tx_fifo = &s_pc104_tx_fifo,
    .tx_callback = UART0_TxReadyCallback
};
static uart_dev_t s_console_device =
{
    .number = UART_NUMBER_CONSOLE,
    .status = UART_STATUS_STOPPED,
    .rx_fifo = &s_console_rx_fifo,
    .tx_fifo = &s_console_tx_fifo
};

bool uart_platform_can_write(const uart_dev_t *dev)
{
    if (dev == &s_uart0_device && dev->number == UART_NUMBER_UART0)
    {
        return true;
    }
    if (dev == &s_console_device && dev->number == UART_NUMBER_CONSOLE)
    {
        return true;
    }
    return false;
}

void uart_platform_start_tx(uart_dev_t *dev)
{
    if (dev == &s_uart0_device && dev->number == UART_NUMBER_UART0)
    {
        UART0_TxStart();
    }
    /* Timer2 continuously polls the console TX FIFO. */
}

bool BSP_UART0_Init(uint32_t cpu_hz, uint32_t baudrate)
{
    if (s_uart0_initialized)
    {
        return false;
    }

    s_uart0_device.baudrate = baudrate;
    s_uart0_device.status = UART_STATUS_STOPPED;
    s_uart0_device.tx_callback = UART0_TxReadyCallback;

    if (!FIFO_Init(&s_pc104_rx_fifo, s_pc104_rx_storage,
                   sizeof(s_pc104_rx_storage)) ||
        !FIFO_Init(&s_pc104_tx_fifo, s_pc104_tx_storage,
                   sizeof(s_pc104_tx_storage)))
    {
        return false;
    }

    BSP_UART0_ConfigurePins();
    if (!UART0_Init(cpu_hz, baudrate, &s_pc104_rx_fifo, &s_pc104_tx_fifo))
    {
        return false;
    }

    s_uart0_device.status = UART_STATUS_READY;
    s_uart0_initialized = true;
    return true;
}

void BSP_UART0_Deinit(void)
{
    s_uart0_device.status = UART_STATUS_STOPPED;
    if (s_uart0_initialized)
    {
        UART0_Deinit();
        s_uart0_initialized = false;
    }
    s_uart0_device.rx_callback = NULL;
    s_uart0_device.callback_user = NULL;
}

uart_dev_t *BSP_UART0_Device(void)
{
    return s_uart0_initialized ? &s_uart0_device : NULL;
}

bool BSP_UART_InitBoard(void)
{
    return BSP_UART_Init(F_CPU, UART_XTP_BAUD, SOFTUART_BAUD);
}

bool BSP_UART_Init(uint32_t cpu_hz, uint32_t pc104_baud, uint32_t console_baud)
{
    if (s_uart0_initialized || s_console_initialized ||
        !BSP_UART0_Init(cpu_hz, pc104_baud))
    {
        return false;
    }

    s_console_device.baudrate = console_baud;
    s_console_device.status = UART_STATUS_STOPPED;
    if (!FIFO_Init(&s_console_rx_fifo, s_console_rx_storage,
                   sizeof(s_console_rx_storage)) ||
        !FIFO_Init(&s_console_tx_fifo, s_console_tx_storage,
                   sizeof(s_console_tx_storage)))
    {
        BSP_UART0_Deinit();
        return false;
    }
    if (!ConsoleUART_Init(cpu_hz, console_baud,
                          &s_console_rx_fifo, &s_console_tx_fifo))
    {
        BSP_UART0_Deinit();
        return false;
    }

    s_console_device.status = UART_STATUS_READY;
    s_console_initialized = true;
    return true;
}

void BSP_UART_Deinit(void)
{
    s_console_device.status = UART_STATUS_STOPPED;
    if (s_console_initialized)
    {
        ConsoleUART_Deinit();
        s_console_initialized = false;
    }
    s_console_device.rx_callback = NULL;
    s_console_device.callback_user = NULL;
    BSP_UART0_Deinit();
}

uart_dev_t *BSP_UART_PC104(void)
{
    return BSP_UART0_Device();
}

uart_dev_t *BSP_UART_Console(void)
{
    return s_console_initialized ? &s_console_device : NULL;
}

static uart_dev_t *BSP_UART_DeviceByNumber(uart_number_t number)
{
    switch (number)
    {
        case UART_NUMBER_UART0:
            return BSP_UART0_Device();
        case UART_NUMBER_CONSOLE:
            return BSP_UART_Console();
        default:
            return NULL;
    }
}

bool BSP_UART_Write(uart_number_t number, uint8_t value)
{
    return uart_write(BSP_UART_DeviceByNumber(number), value);
}

bool BSP_UART_Read(uart_number_t number, uint8_t *value)
{
    return uart_read(BSP_UART_DeviceByNumber(number), value);
}

ISR(USART0_RX_vect)
{
    if (UART0_OnRxInterrupt() &&
        s_uart0_device.status == UART_STATUS_READY &&
        s_uart0_device.rx_callback != NULL)
    {
        s_uart0_device.rx_callback(&s_uart0_device,
                                   s_uart0_device.callback_user);
    }
}

ISR(USART0_UDRE_vect)
{
    uart_tx_callback_t callback = s_uart0_device.tx_callback;
    if (callback == NULL)
    {
        callback = UART0_TxReadyCallback;
    }
    callback(&s_uart0_device);
}

ISR(TIMER2_COMP_vect)
{
    if (SoftUART_OnTimerInterrupt() &&
        s_console_device.status == UART_STATUS_READY &&
        s_console_device.rx_callback != NULL)
    {
        s_console_device.rx_callback(&s_console_device,
                                     s_console_device.callback_user);
    }
}
