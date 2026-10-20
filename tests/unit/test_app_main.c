#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <avr/io.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_main.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/LED/bsp_led.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/UART/bsp_uart.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Timer/bsp_timer.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Power/bsp_power.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Scheduler/scheduler.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_watchdog.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_power.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_mode.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NVM/bsp_nvm.h"

/* This test exercises scheduler startup; the RTU stack has its own test. */
bool AppModbus_Init(void) { return true; }
void ADC_Init(void) { }
/* Simulated bus: the TPS55287 (0x74, 0x75), PAC1934 (0x1F), and 24C04 (0x50) acknowledge.
 * The PAC1934 returns its product ID for register 0xFD. */
static uint8_t i2c_selected;
static uint8_t i2c_register;
static bool i2c_awaiting_data;
/* Only the TPS55287's VOUT_FS register (0x04) needs real write-then-readback persistence,
 * for AppPower_SetTrans's internal-feedback verification. Every other register/device
 * keeps the old always-0x00 (or fixed product ID) behavior. */
static uint8_t tps_vout_fs = 0x03U; /* POR default: internal feedback, FB_SEL=3 */

bool I2C_AVR_Init(uint32_t cpu_hz, uint32_t bus_hz)
{
    (void)cpu_hz;
    (void)bus_hz;
    TWBR = 72U;
    TWCR |= _BV(TWEN);
    return true;
}

bool I2C_AVR_Start(uint8_t address_rw)
{
    i2c_selected = (uint8_t)(address_rw >> 1);
    i2c_awaiting_data = false;
    return i2c_selected == 0x74U || i2c_selected == 0x75U ||
           i2c_selected == 0x1FU || i2c_selected == 0x50U;
}

bool I2C_AVR_Write(uint8_t value)
{
    if (!i2c_awaiting_data)
    {
        i2c_register = value;
        i2c_awaiting_data = true;
    }
    else if ((i2c_selected == 0x74U || i2c_selected == 0x75U) && i2c_register == 0x04U)
    {
        tps_vout_fs = value;
    }
    return true;
}

bool I2C_AVR_Read(uint8_t *value, bool ack)
{
    (void)ack;
    if ((i2c_selected == 0x74U || i2c_selected == 0x75U) && i2c_register == 0x04U)
        *value = tps_vout_fs;
    else
        *value = (i2c_selected == 0x1FU && i2c_register == 0xFDU) ? 0x5BU : 0x00U;
    return true;
}

void I2C_AVR_Stop(void)
{
}

static uint8_t nvm_fault_bits;
uint8_t BSP_NVM_ReadFaultLatch(void) { return nvm_fault_bits; }
void BSP_NVM_WriteFaultLatch(uint8_t bits) { nvm_fault_bits = bits; }
bool BSP_NVM_ReadLimits(BSP_NVM_Limits *limits) { (void)limits; return false; }
void BSP_NVM_WriteLimits(const BSP_NVM_Limits *limits) { (void)limits; }
bool BSP_NVM_ReadLimitError(void) { return false; }
void BSP_NVM_WriteLimitError(bool set) { (void)set; }
bool BSP_NVM_ReadBootRecord(BSP_NVM_BootRecord *record) { (void)record; return false; }
void BSP_NVM_WriteBootRecord(const BSP_NVM_BootRecord *record) { (void)record; }
uint8_t BSP_NVM_BootHistoryNext(void) { return 0U; }
void BSP_NVM_SetBootHistoryNext(uint8_t next) { (void)next; }
bool BSP_NVM_ReadBootHistory(uint8_t slot, BSP_NVM_BootRecord *record) { (void)slot; (void)record; return false; }
void BSP_NVM_WriteBootHistory(uint8_t slot, const BSP_NVM_BootRecord *record) { (void)slot; (void)record; }
bool BSP_NVM_ReadConfig(BSP_NVM_Config *config) { (void)config; return false; }
void BSP_NVM_WriteConfig(const BSP_NVM_Config *config) { (void)config; }
void BSP_NVM_EraseAll(void) { nvm_fault_bits = 0U; }
bool ADC_Read(uint8_t channel, uint16_t *counts)
{
    (void)channel;
    *counts = 154U; /* 25.1 C */
    return true;
}
static unsigned int modbus_polls;
void AppModbus_Update(void) { modbus_polls++; }

volatile uint8_t UCSR0A, UCSR0B, UCSR0C, UBRR0H, UBRR0L, UDR0;
volatile uint8_t TIMSK, TCCR0, OCR0, TCNT0, TCCR2, OCR2, TCNT2, TIFR;
volatile uint8_t DDRE, PORTE, PINE;
volatile uint8_t DDRA, PORTA;
volatile uint8_t DDRB, PORTB, DDRC, PORTC, DDRD, PORTD;
volatile uint8_t PINC;
volatile uint8_t PINB;
volatile uint8_t DDRF, PORTF, DDRG, PORTG;
volatile uint8_t MCUCR;
volatile uint8_t TWBR, TWSR, TWDR, TWCR;

static unsigned int failures;

void TIMER0_COMP_vect(void);

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expression); \
        failures++; \
    } \
} while (0)

static void check_message(uart_dev_t *dev, const char *expected)
{
    uint8_t value = 0U;
    size_t index;

    CHECK(dev != NULL);
    if (dev == NULL)
    {
        return;
    }

    CHECK(FIFO_Count(dev->tx_fifo) == strlen(expected));
    for (index = 0U; expected[index] != '\0'; index++)
    {
        CHECK(FIFO_Pop(dev->tx_fifo, &value));
        CHECK(value == (uint8_t)expected[index]);
    }
}

bool I2C_AVR_TimedOut(void)
{
    return false;
}

void I2C_AVR_ClearTimeout(void)
{
}

void I2C_AVR_Recover(void)
{
}

static void run_ms(unsigned int count)
{
    while (count-- > 0U)
    {
        TIMER0_COMP_vect();
        SCH_HandleScheduledTask();
    }
}

static void send_uart(uart_dev_t *dev, const char *message)
{
    while (*message != '\0')
    {
        CHECK(FIFO_Push(dev->rx_fifo, (uint8_t)*message));
        message++;
    }
}

static void send_console(const char *message)
{
    send_uart(BSP_UART_Console(), message);
}

static void read_uart_output(uart_dev_t *dev, char *output, size_t capacity)
{
    uint8_t byte;
    size_t length = 0U;
    while (length + 1U < capacity && FIFO_Pop(dev->tx_fifo, &byte))
    {
        output[length++] = (char)byte;
    }
    output[length] = '\0';
}

static void read_console_output(char *output, size_t capacity)
{
    read_uart_output(BSP_UART_Console(), output, capacity);
}

static size_t read_uart_bytes(uart_dev_t *dev, uint8_t *output, size_t capacity)
{
    uint8_t byte;
    size_t length = 0U;
    while (length < capacity && FIFO_Pop(dev->tx_fifo, &byte)) output[length++] = byte;
    return length;
}

int main(void)
{
    uart_dev_t *uart0;
    uart_dev_t *console;

    PINE = _BV(PE4); /* Idle console RX line. */
    PINB = 0U;
    PINC = _BV(PC4) | _BV(PC0);
    DDRA = _BV(1);
    PORTA = _BV(1) | _BV(PA3);
    MCUCR = _BV(SRE) | _BV(2);
    DDRE = _BV(PE0) | _BV(PE4);
    PORTE = _BV(PE0);
    CHECK(APP_Initialize());
    uart0 = BSP_UART0_Device();
    console = BSP_UART_Console();

    CHECK(FIFO_Count(uart0->tx_fifo) == 0U);
    check_message(console, "Console test message\r\n");
    if (uart0 != NULL)
    {
        CHECK(uart0->baudrate == 115200UL);
    }
    if (console != NULL)
    {
        CHECK(console->baudrate == 9600UL);
    }
    CHECK((UCSR0B & _BV(UDRIE0)) == 0U);
    CHECK((TIMSK & _BV(OCIE2)) != 0U);
    CHECK((TIMSK & _BV(OCIE0)) != 0U);
    CHECK(TCCR0 == (_BV(WGM01) | _BV(CS01) | _BV(CS00)));
    CHECK(OCR0 == 186U);
    CHECK(UBRR0H == 0U && UBRR0L == 12U);
    CHECK(TWBR == 72U && (TWCR & _BV(TWEN)) != 0U);
    CHECK((DDRD & (_BV(PD0) | _BV(PD1))) == 0U);
    CHECK((PORTC & _BV(PC2)) != 0U);
    CHECK((DDRE & _BV(PE0)) == 0U);
    CHECK((PORTE & _BV(PE0)) == 0U);
    CHECK((DDRE & (_BV(PE1) | _BV(PE3))) ==
          (_BV(PE1) | _BV(PE3)));
    CHECK((PORTE & (_BV(PE1) | _BV(PE3) | _BV(PE4))) ==
          (_BV(PE1) | _BV(PE3) | _BV(PE4)));
    CHECK((DDRE & _BV(PE4)) == 0U);
    CHECK(DDRA == (_BV(1) | _BV(PA0) | _BV(PA3)));
    CHECK(PORTA == (_BV(1) | _BV(PA0) | _BV(PA3))); /* WP is protected; heartbeat starts ON. */
    CHECK((DDRB & _BV(PB5)) == 0U);
    CHECK((DDRB & _BV(PB6)) != 0U);
    CHECK((PORTB & (_BV(PB5) | _BV(PB6))) == 0U);
    CHECK(!AppWatchdog_IsFaulted());
    CHECK(MCUCR == _BV(2)); /* PORTA is no longer owned by XMEM. */
    CHECK(SystemData_GetHeartbeatTiming().on_ms == 1000U);
    CHECK(SystemData_GetHeartbeatTiming().off_ms == 1000U);
    CHECK(SCH_SystemTick() == 0U);
    run_ms(9U);
    CHECK(SCH_SystemTick() == 9U);
    CHECK(FIFO_Count(console->tx_fifo) == 0U);
    run_ms(1U);
    check_message(console, "> ");
    CHECK(FIFO_Count(uart0->tx_fifo) == 0U);

    send_uart(uart0, "version\r");
    run_ms(10U);
    {
        char output[256];
        read_uart_output(uart0, output, sizeof(output));
        CHECK(output[0] == '\0');
        CHECK(FIFO_Count(console->tx_fifo) == 0U);
    }

    /* UART0 text is ignored; the software UART retains its own CLI line. */
    send_console("ver");
    send_uart(uart0, "led ");
    run_ms(10U);
    send_console("sion\r");
    send_uart(uart0, "get\r");
    run_ms(10U);
    run_ms(10U);
    {
        char soft_output[256];
        char uart0_output[256];
        read_console_output(soft_output, sizeof(soft_output));
        read_uart_output(uart0, uart0_output, sizeof(uart0_output));
        CHECK(strstr(soft_output, "version\r\nDAL_PDU firmware v") != NULL);
        CHECK(strstr(soft_output, "LED on=") == NULL);
        CHECK(uart0_output[0] == '\0');
    }

    send_console("version\r\n");
    run_ms(10U);
    {
        char output[256];
        read_console_output(output, sizeof(output));
        CHECK(strstr(output, "version\r\nDAL_PDU firmware v") != NULL);
        CHECK(strstr(output, "\r\n> ") != NULL);
    }
    CHECK((PORTB & _BV(PB6)) == 0U); /* First feed is due after one second. */
    send_console("versionx\b\r");
    run_ms(10U);
    {
        char output[256];
        read_console_output(output, sizeof(output));
        CHECK(strstr(output, "\b \b\r\nDAL_PDU firmware v") != NULL);
    }
    send_console("bad\r");
    run_ms(10U);
    {
        char output[256];
        read_console_output(output, sizeof(output));
        CHECK(strstr(output, "Unknown command\r\n> ") != NULL);
    }
    send_console("help\r");
    run_ms(10U);
    {
        char output[2048] = {0};
        size_t length = 0U;
        unsigned int iteration;
        for (iteration = 0U; iteration < 30U; iteration++)
        {
            char chunk[129];
            read_console_output(chunk, sizeof(chunk));
            if (length + strlen(chunk) < sizeof(output))
            {
                strcpy(output + length, chunk);
                length += strlen(chunk);
            }
            run_ms(10U);
        }
        CHECK(strstr(output, "Available commands") != NULL);
        CHECK(strstr(output, "version : Display firmware version") != NULL);
        CHECK(strstr(output, "tps55287 : U5 VOUT") != NULL);
        CHECK(strstr(output, "pac1934 : Rail monitor") != NULL);
        CHECK(strstr(output, "eeprom : U14 24C04") != NULL);
        CHECK(strstr(output, "[output truncated]") == NULL);
        CHECK(strstr(output, "> ") != NULL);
    }
    CHECK(PORTA == (_BV(1) | _BV(PA0) | _BV(PA3)));
    CHECK((PORTB & _BV(PB6)) == 0U); /* First periodic pulse is not due yet. */
    run_ms(250U);
    CHECK(SCH_SystemTick() == 640U);
    CHECK(PORTA == (_BV(1) | _BV(PA0) | _BV(PA3)));
    run_ms(360U);
    CHECK(SCH_SystemTick() == 1000U);
    CHECK((PORTB & _BV(PB6)) != 0U); /* DONE pulse at one second. */
    CHECK(PORTA == (_BV(1) | _BV(PA0)));
    run_ms(1000U);
    CHECK(SCH_SystemTick() == 2000U);
    CHECK((PORTB & _BV(PB6)) != 0U); /* Next DONE pulse at two seconds. */
    CHECK(PORTA == (_BV(1) | _BV(PA0) | _BV(PA3)));

    send_console("led set 200 400\r");
    run_ms(10U);
    {
        char output[256];
        read_console_output(output, sizeof(output));
        CHECK(strstr(output, "LED on=200 ms off=400 ms\r\n> ") != NULL);
    }
    CHECK(SystemData_GetHeartbeatTiming().on_ms == 200U);
    CHECK(SystemData_GetHeartbeatTiming().off_ms == 400U);
    run_ms(10U); /* The 20 ms heartbeat task applies the new timing. */
    run_ms(199U);
    CHECK(PORTA == (_BV(1) | _BV(PA0) | _BV(PA3)));
    run_ms(1U);
    CHECK(PORTA == (_BV(1) | _BV(PA0)));
    run_ms(399U);
    CHECK(PORTA == (_BV(1) | _BV(PA0)));
    run_ms(1U);
    CHECK(PORTA == (_BV(1) | _BV(PA0) | _BV(PA3)));
    send_console("trans on\r");
    run_ms(10U);
    CHECK(BSP_Power_IsTransOn());
    CHECK((PORTC & _BV(PC2)) == 0U);
    while ((SCH_SystemTick() % 6U) != 3U) run_ms(1U);
    PINC &= (uint8_t)~_BV(PC4);
    run_ms(40U);
    CHECK(BSP_Power_IsTransOn()); /* Still inside the 50 ms debounce window. */
    run_ms(20U); /* Now well past 50 ms since the pin went active. */
    CHECK(BSP_Power_IsTransOn());
    CHECK((PORTC & _BV(PC2)) == 0U);
    CHECK(SystemData_GetPowerStatus().fault_pc4);
    CHECK(AppMode_Get() == APP_MODE_WARNING);
    CHECK(AppPower_SetTrans(true));
    PINC |= _BV(PC4);
    CHECK(AppPower_SetTrans(true)); /* Latch is retained for reporting. */
    CHECK(AppPower_ClearFaultLatch());
    CHECK(AppPower_SetTrans(true));
    CHECK(SystemData_GetPowerStatus().trans_on);
    PINC &= (uint8_t)~_BV(PC0);
    run_ms(60U);
    CHECK(SystemData_GetPowerStatus().trans_on);
    CHECK(SystemData_GetPowerStatus().fault_pc0);
    CHECK((PORTC & _BV(PC2)) == 0U);
    CHECK(AppMode_Get() == APP_MODE_WARNING);
    PINC |= _BV(PC0);
    CHECK(AppPower_ClearFaultLatch());

    send_console("mode retry\r");
    run_ms(10U);
    {
        char output[256];
        read_console_output(output, sizeof(output));
        CHECK(strstr(output, "MODE retry") != NULL);
    }
    AppMode_SetRegulatorFault();
    send_console("mode normal\r");
    run_ms(10U);
    {
        char output[256];
        read_console_output(output, sizeof(output));
        CHECK(strstr(output, "MODE normal") != NULL);
        CHECK(!AppMode_RegulatorError());
    }

    /* A second software-UART command still works after UART0 receives text. */
    send_uart(uart0, "help\r");
    run_ms(10U);
    send_console("version\r");
    run_ms(10U);
    {
        char soft_output[256];
        read_console_output(soft_output, sizeof(soft_output));
        CHECK(strstr(soft_output, "DAL_PDU firmware v") != NULL);
        CHECK(strstr(soft_output, "> ") != NULL);
    }
    CHECK(FIFO_Count(uart0->tx_fifo) == 0U);
    {
        /* Modbus runs as a 5 ms task: 100 ms gives about 20 passes, each with two polls inside. */
        unsigned int before = modbus_polls;
        run_ms(100U);
        CHECK(modbus_polls - before >= 19U && modbus_polls - before <= 21U);
    }
    {
        /* Captured xbld_client-compatible xTP 0xA2 / xCLI ping and reboot frames. */
        static const uint8_t ping[] = {
            0xC0U,0xDEU,0xA2U,0x00U,0x01U,0x03U,0x09U,0x00U,0x02U,0x07U,
            0x70U,0x69U,0x6EU,0x67U,0x20U,0x30U,0x05U,0xA0U,0xA9U,0xFCU,0xDAU,0xEDU
        };
        static const uint8_t reboot[] = {
            0xC0U,0xDEU,0xA2U,0x00U,0x01U,0x03U,0x0BU,0x00U,0x04U,0x07U,
            0x72U,0x65U,0x62U,0x6FU,0x6FU,0x74U,0x20U,0x30U,0x12U,0xD7U,
            0xD7U,0x7CU,0xDAU,0xEDU
        };
        uint8_t bytes[64];
        size_t j, length;
        for (j = 0U; j < sizeof(ping); j++)
            CHECK(FIFO_Push(uart0->rx_fifo, (j == 18U) ? 0U : ping[j]));
        run_ms(10U);
        CHECK(FIFO_Count(uart0->tx_fifo) == 0U); /* Bad CRC is ignored. */
        for (j = 0U; j < sizeof(ping); j++)
            CHECK(FIFO_Push(uart0->rx_fifo, (j == 10U) ? (uint8_t)(ping[j] ^ 1U) : ping[j]));
        run_ms(10U);
        CHECK(read_uart_bytes(uart0, bytes, sizeof(bytes)) == 44U); /* ECC repairs payload. */
        for (j = 0U; j < sizeof(ping); j++) CHECK(FIFO_Push(uart0->rx_fifo, ping[j]));
        run_ms(10U);
        length = read_uart_bytes(uart0, bytes, sizeof(bytes));
        CHECK(length == 44U); /* 14-byte ARQ ACK and 30-byte XCLI response. */
        CHECK(bytes[0] == 0xC0U && bytes[3] == 0xFEU && bytes[4] == 0xFFU);
        CHECK(bytes[7] == 0x03U && bytes[14] == 0xC0U && bytes[17] == 0x01U);
        CHECK(bytes[21] == 0x01U && bytes[22] == 0x02U && bytes[23] == 0x07U);
        CHECK(bytes[24] == 0x00U && bytes[27] == 0x04U); /* mode=app TLV. */
        for (j = 0U; j < sizeof(reboot); j++) CHECK(FIFO_Push(uart0->rx_fifo, reboot[j]));
        run_ms(10U);
        length = read_uart_bytes(uart0, bytes, sizeof(bytes));
        CHECK(length == 33U); /* ARQ ACK and empty successful XCLI response. */
        CHECK(bytes[17] == 0x01U && bytes[21] == 0x01U && bytes[22] == 0x04U);
    }
    BSP_Timer_Deinit();
    CHECK((TIMSK & _BV(OCIE0)) == 0U);
    BSP_UART_Deinit();

    if (failures != 0U)
    {
        fprintf(stderr, "%u app startup checks failed\n", failures);
        return 1;
    }

    puts("App startup tests passed");
    return 0;
}
