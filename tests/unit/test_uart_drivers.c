#include <stdio.h>
#include <stdint.h>

#include <avr/io.h>

#include "uart0.h"
#include "soft_uart.h"
#include "console_uart.h"
#include "bsp_uart.h"

volatile uint8_t UCSR0A, UCSR0B, UCSR0C, UBRR0H, UBRR0L, UDR0;
volatile uint8_t TIMSK, TCCR2, OCR2, TCNT2, TIFR;
volatile uint8_t DDRE, PORTE, PINE;

void USART0_RX_vect(void);
void USART0_UDRE_vect(void);
void TIMER2_COMP_vect(void);

static unsigned int failures;
static unsigned int callback_calls;

#define SOFT_UART_TEST_SAMPLES_PER_BIT 3U

static void test_rx_callback(uart_dev_t *dev, void *user)
{
    uint8_t *received = (uint8_t *)user;
    callback_calls++;
    (void)uart_read(dev, received);
}

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expression); \
        failures++; \
    } \
} while (0)

static void tick_n(uint8_t count)
{
    while (count-- > 0U)
    {
        TIMER2_COMP_vect();
    }
}

static void send_rx_frame(uint8_t value, uint8_t stop_high)
{
    uint8_t bit;

    PINE &= (uint8_t)~_BV(PE4);
    tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
    for (bit = 0U; bit < 8U; bit++)
    {
        if ((value & (1U << bit)) != 0U)
        {
            PINE |= _BV(PE4);
        }
        else
        {
            PINE &= (uint8_t)~_BV(PE4);
        }
        tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
    }
    if (stop_high != 0U)
    {
        PINE |= _BV(PE4);
    }
    else
    {
        PINE &= (uint8_t)~_BV(PE4);
    }
    tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
    PINE |= _BV(PE4);
    tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
}

static void send_rx_stream(const uint8_t *values, uint8_t length)
{
    uint8_t index;
    uint8_t bit;

    PINE |= _BV(PE4);
    tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
    for (index = 0U; index < length; index++)
    {
        PINE &= (uint8_t)~_BV(PE4);
        tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((values[index] & (1U << bit)) != 0U) PINE |= _BV(PE4);
            else PINE &= (uint8_t)~_BV(PE4);
            tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
        }
        PINE |= _BV(PE4);
        tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
    }
}

static void test_uart0_fifo_and_interrupts(void)
{
    FIFO_t rx, tx;
    uart_dev_t dev = { 0 };
    uint8_t rx_storage[1], tx_storage[2];
    uint8_t value = 0U;

    CHECK(FIFO_Init(&rx, rx_storage, sizeof(rx_storage)));
    CHECK(FIFO_Init(&tx, tx_storage, sizeof(tx_storage)));
    CHECK(!UART0_Init(12000000UL, 0U, &rx, &tx));
    CHECK(UART0_Init(12000000UL, 9600U, &rx, &tx));
    CHECK(UBRR0H == 0U && UBRR0L == 155U);
    CHECK((UCSR0A & _BV(U2X0)) != 0U);
    CHECK((UCSR0B & (_BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0))) ==
          (_BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0)));

    dev.tx_fifo = &tx;
    CHECK(FIFO_Push(&tx, 0x41U));
    CHECK((UCSR0B & _BV(UDRIE0)) == 0U);
    UART0_TxStart();
    CHECK((UCSR0B & _BV(UDRIE0)) != 0U);
    UART0_TxReadyCallback(&dev);
    CHECK(UDR0 == 0x41U);
    UART0_TxReadyCallback(&dev);
    CHECK((UCSR0B & _BV(UDRIE0)) == 0U);

    UCSR0A = 0U;
    UDR0 = 0x55U;
    USART0_RX_vect();
    UDR0 = 0x66U;
    USART0_RX_vect();
    UCSR0A = _BV(FE0);
    UDR0 = 0x77U;
    USART0_RX_vect();
    CHECK(UART0_RxDropped() == 1U);
    CHECK(UART0_RxErrors() == 1U);
    CHECK(UART0_Read(&value));
    CHECK(value == 0x55U);
    CHECK(!UART0_Read(&value));

    UART0_Deinit();
    CHECK(UCSR0B == 0U);
    UART0_TxStart();
    CHECK((UCSR0B & _BV(UDRIE0)) == 0U);
}

static void test_portable_interface_and_bsp_binding(void)
{
    uart_dev_t *pc104;
    uart_dev_t *console;
    uint8_t value = 0U;
    uint8_t callback_byte = 0U;

    CHECK(!uart_write(NULL, 1U));
    CHECK(!uart_read(NULL, &value));

    CHECK(BSP_UART_PC104() == NULL);
    CHECK(BSP_UART_Console() == NULL);
    PINE = _BV(PE4);
    CHECK(!BSP_UART_Init(12000000UL, 9600U, 115200U));
    CHECK(BSP_UART_PC104() == NULL);
    CHECK(UCSR0B == 0U);
    CHECK(BSP_UART_Init(12000000UL, 9600U, 9600U));
    CHECK(!BSP_UART_Init(12000000UL, 9600U, 9600U));
    pc104 = BSP_UART_PC104();
    console = BSP_UART_Console();
    CHECK(pc104 != NULL && console != NULL);
    CHECK(pc104->number == UART_NUMBER_PC104);
    CHECK(pc104->baudrate == 9600U);
    CHECK(pc104->status == UART_STATUS_READY);
    CHECK(pc104->rx_fifo != NULL && pc104->tx_fifo != NULL);
    CHECK(console->number == UART_NUMBER_CONSOLE);
    CHECK(console->baudrate == 9600U);
    CHECK(!uart_read(pc104, NULL));
    pc104->number = UART_NUMBER_CONSOLE;
    CHECK(!uart_write(pc104, 0x31U));
    pc104->number = UART_NUMBER_PC104;
    CHECK(FIFO_Count(pc104->tx_fifo) == 0U);

    CHECK(uart_write(pc104, 0x31U));
    CHECK(uart_write(pc104, 0x32U));
    CHECK(FIFO_Count(pc104->tx_fifo) == 2U);
    CHECK((UCSR0B & _BV(UDRIE0)) != 0U);
    USART0_UDRE_vect();
    CHECK(UDR0 == 0x31U);
    CHECK(FIFO_Count(pc104->tx_fifo) == 1U);
    USART0_UDRE_vect();
    CHECK(UDR0 == 0x32U);
    CHECK(FIFO_Count(pc104->tx_fifo) == 0U);
    USART0_UDRE_vect();
    CHECK((UCSR0B & _BV(UDRIE0)) == 0U);
    CHECK(uart_write(pc104, 0x33U));
    CHECK((UCSR0B & _BV(UDRIE0)) != 0U);
    USART0_UDRE_vect();
    CHECK(UDR0 == 0x33U);
    USART0_UDRE_vect();
    CHECK((UCSR0B & _BV(UDRIE0)) == 0U);
    uart_set_rx_callback(pc104, test_rx_callback, &callback_byte);
    callback_calls = 0U;
    UCSR0A = 0U;
    UDR0 = 0x42U;
    USART0_RX_vect();
    CHECK(callback_calls == 1U);
    CHECK(callback_byte == 0x42U);
    CHECK(!uart_read(pc104, &value));
    UCSR0A = _BV(FE0);
    UDR0 = 0x43U;
    USART0_RX_vect();
    CHECK(callback_calls == 1U);

    CHECK(uart_write(console, 0x55U));
    tick_n(1U);
    CHECK((PORTE & _BV(PE3)) == 0U);
    uart_set_rx_callback(console, test_rx_callback, &callback_byte);
    send_rx_frame(0x66U, 1U);
    CHECK(callback_calls == 2U);
    CHECK(callback_byte == 0x66U);
    send_rx_frame(0x77U, 0U);
    CHECK(callback_calls == 2U);

    BSP_UART_Deinit();
    CHECK(BSP_UART_PC104() == NULL);
    CHECK(BSP_UART_Console() == NULL);
    CHECK(pc104->status == UART_STATUS_STOPPED);
    CHECK(!uart_write(pc104, 0x31U));
}

static void test_console_soft_uart_tx_rx(void)
{
    FIFO_t rx, tx;
    uint8_t rx_storage[1], tx_storage[2];
    uint8_t value = 0U;
    uint8_t bit;

    DDRE = 0U;
    PORTE = 0U;
    PINE = _BV(PE4);
    CHECK(FIFO_Init(&rx, rx_storage, sizeof(rx_storage)));
    CHECK(FIFO_Init(&tx, tx_storage, sizeof(tx_storage)));
    CHECK(!ConsoleUART_Init(12000000UL, 115200U, &rx, &tx));
    CHECK(ConsoleUART_Init(12000000UL, 2400U, &rx, &tx));
    CHECK(OCR2 == 207U);
    CHECK(TCCR2 == (_BV(WGM21) | _BV(CS21)));
    ConsoleUART_Deinit();
    CHECK(ConsoleUART_Init(12000000UL, 9600U, &rx, &tx));
    CHECK(OCR2 == 51U);
    CHECK(TCCR2 == (_BV(WGM21) | _BV(CS21)));
    CHECK((DDRE & _BV(PE3)) != 0U);
    CHECK((DDRE & _BV(PE4)) == 0U);
    CHECK((PORTE & (_BV(PE3) | _BV(PE4))) == (_BV(PE3) | _BV(PE4)));
    CHECK((TIMSK & _BV(OCIE2)) != 0U);
    CHECK(SoftUART_TxIdle());

    CHECK(ConsoleUART_Write(0xA5U));
    CHECK(!SoftUART_TxIdle());
    tick_n(1U);
    CHECK((PORTE & _BV(PE3)) == 0U); /* Start bit. */
    for (bit = 0U; bit < 8U; bit++)
    {
        tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
        CHECK(((PORTE & _BV(PE3)) != 0U) == ((0xA5U & (1U << bit)) != 0U));
    }
    tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
    CHECK((PORTE & _BV(PE3)) != 0U); /* Stop bit. */
    tick_n(SOFT_UART_TEST_SAMPLES_PER_BIT);
    CHECK(SoftUART_TxIdle());

    send_rx_frame(0x5AU, 1U);
    CHECK(ConsoleUART_Read(&value));
    CHECK(value == 0x5AU);
    send_rx_frame(0x33U, 0U);
    CHECK(SoftUART_RxFramingErrors() == 1U);
    CHECK(!ConsoleUART_Read(&value));
    send_rx_frame(0x12U, 1U);
    send_rx_frame(0x34U, 1U);
    CHECK(SoftUART_RxDropped() == 1U);
    CHECK(ConsoleUART_Read(&value));
    CHECK(value == 0x12U);

    ConsoleUART_Deinit();
    CHECK((TIMSK & _BV(OCIE2)) == 0U);
    CHECK(!ConsoleUART_Write(0x55U));
}

static void test_bsp_uart0_only(void)
{
    uart_dev_t *dev;

    CHECK(BSP_UART0_Device() == NULL);
    CHECK(!BSP_UART0_Init(12000000UL, 0U));
    CHECK(BSP_UART0_Device() == NULL);
    CHECK(BSP_UART0_Init(12000000UL, 19200U));
    CHECK(!BSP_UART0_Init(12000000UL, 19200U));

    dev = BSP_UART0_Device();
    CHECK(dev != NULL);
    CHECK(BSP_UART_PC104() == dev);
    CHECK(BSP_UART_Console() == NULL);
    CHECK(dev->number == UART_NUMBER_UART0);
    CHECK(dev->baudrate == 19200U);
    CHECK(dev->status == UART_STATUS_READY);
    CHECK(dev->rx_fifo != NULL && dev->tx_fifo != NULL);
    CHECK(dev->rx_fifo->capacity == 128U);
    CHECK(dev->tx_callback == UART0_TxReadyCallback);
    CHECK(dev->rx_callback == NULL);

    CHECK(uart_write(dev, 0x5AU));
    CHECK((UCSR0B & _BV(UDRIE0)) != 0U);
    USART0_UDRE_vect();
    CHECK(UDR0 == 0x5AU);
    USART0_UDRE_vect();
    CHECK((UCSR0B & _BV(UDRIE0)) == 0U);

    BSP_UART0_Deinit();
    CHECK(dev->status == UART_STATUS_STOPPED);
    CHECK(BSP_UART0_Device() == NULL);
    CHECK(BSP_UART_PC104() == NULL);
    CHECK(!uart_write(dev, 0x5AU));
}

static void test_soft_uart_continuous_rx_with_tx(void)
{
    static const uint8_t message[] = {'h', 'e', 'l', 'l', 'o', '\r'};
    FIFO_t rx, tx;
    uint8_t rx_storage[16], tx_storage[8];
    uint8_t index;
    uint8_t value = 0U;

    PINE = _BV(PE4);
    CHECK(FIFO_Init(&rx, rx_storage, sizeof(rx_storage)));
    CHECK(FIFO_Init(&tx, tx_storage, sizeof(tx_storage)));
    CHECK(ConsoleUART_Init(12000000UL, 9600U, &rx, &tx));
    CHECK(ConsoleUART_Write('O'));
    CHECK(ConsoleUART_Write('K'));
    send_rx_stream(message, (uint8_t)sizeof(message));
    CHECK(SoftUART_RxDropped() == 0U);
    CHECK(SoftUART_RxFramingErrors() == 0U);
    for (index = 0U; index < sizeof(message); index++)
    {
        CHECK(ConsoleUART_Read(&value));
        CHECK(value == message[index]);
    }
    CHECK(!ConsoleUART_Read(&value));
    ConsoleUART_Deinit();
}

static void test_bsp_byte_io(void)
{
    uint8_t value = 0U;
    uart_number_t invalid_number = (uart_number_t)2;

    CHECK(!BSP_UART_Write(UART_NUMBER_UART0, 0x11U));
    CHECK(!BSP_UART_Read(UART_NUMBER_CONSOLE, &value));
    PINE = _BV(PE4);
    CHECK(BSP_UART_InitBoard());
    CHECK(!BSP_UART_Write(invalid_number, 0x11U));
    CHECK(!BSP_UART_Read(invalid_number, &value));
    CHECK(!BSP_UART_Read(UART_NUMBER_UART0, NULL));
    CHECK(!BSP_UART_Read(UART_NUMBER_UART0, &value));

    CHECK(BSP_UART_Write(UART_NUMBER_UART0, 0xA5U));
    CHECK((UCSR0B & _BV(UDRIE0)) != 0U);
    USART0_UDRE_vect();
    CHECK(UDR0 == 0xA5U);
    USART0_UDRE_vect();
    CHECK((UCSR0B & _BV(UDRIE0)) == 0U);

    CHECK(BSP_UART_Write(UART_NUMBER_CONSOLE, 0x5AU));
    CHECK(FIFO_Count(BSP_UART_Console()->tx_fifo) == 1U);
    tick_n(1U);
    CHECK((PORTE & _BV(PE3)) == 0U);

    UCSR0A = 0U;
    UDR0 = 0x31U;
    USART0_RX_vect();
    CHECK(BSP_UART_Read(UART_NUMBER_UART0, &value));
    CHECK(value == 0x31U);
    CHECK(!BSP_UART_Read(UART_NUMBER_UART0, &value));

    send_rx_frame(0x42U, 1U);
    CHECK(BSP_UART_Read(UART_NUMBER_CONSOLE, &value));
    CHECK(value == 0x42U);
    CHECK(!BSP_UART_Read(UART_NUMBER_CONSOLE, &value));

    BSP_UART_Deinit();
    CHECK(!BSP_UART_Write(UART_NUMBER_UART0, 0x11U));
    CHECK(!BSP_UART_Read(UART_NUMBER_CONSOLE, &value));
}

int main(void)
{
    test_uart0_fifo_and_interrupts();
    test_console_soft_uart_tx_rx();
    test_soft_uart_continuous_rx_with_tx();
    test_portable_interface_and_bsp_binding();
    test_bsp_uart0_only();
    test_bsp_byte_io();

    if (failures != 0U)
    {
        fprintf(stderr, "%u UART driver checks failed\n", failures);
        return 1;
    }

    puts("UART driver tests passed");
    return 0;
}
