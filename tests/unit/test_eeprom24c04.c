#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <avr/io.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/EEPROM/bsp_eeprom.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M3_Driver/Devices/EEPROM24C04/eeprom24c04.h"

volatile uint8_t DDRA, PORTA, MCUCR;

static uint8_t s_memory[EEPROM24C04_SIZE_BYTES];
static uint16_t s_cursor;
static uint16_t s_page_addresses[40];
static uint8_t s_page_lengths[40];
static unsigned int s_page_count;
static unsigned int s_busy_count;
static unsigned int s_probe_count;
static uint8_t s_block;
static uint8_t s_data_count;
static bool s_reading;
static bool s_expect_word;
static bool s_fail_data;
static bool s_ignore_writes;
static bool s_never_ready;
static bool s_wp_low_for_data;
static unsigned int s_failures;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expression); \
        s_failures++; \
    } \
} while (0)

bool I2C_AVR_Start(uint8_t address_rw)
{
    uint8_t address = (uint8_t)(address_rw >> 1);
    if (address != 0x50U && address != 0x51U) return false;
    s_block = (uint8_t)(address - 0x50U);
    s_reading = (address_rw & 1U) != 0U;
    if (!s_reading)
    {
        s_expect_word = true;
        s_data_count = 0U;
    }
    return true;
}

bool I2C_AVR_Write(uint8_t value)
{
    if (s_reading) return false;
    if (s_expect_word)
    {
        s_cursor = (uint16_t)((uint16_t)s_block * 256U + value);
        s_expect_word = false;
        return true;
    }
    if (s_fail_data) return false;
    if (s_data_count == 0U && s_page_count < 40U)
        s_page_addresses[s_page_count] = s_cursor;
    if ((PORTA & _BV(PA0)) == 0U) s_wp_low_for_data = true;
    if (!s_ignore_writes && (PORTA & _BV(PA0)) == 0U)
        s_memory[s_cursor] = value;
    s_cursor = (uint16_t)((uint16_t)s_block * 256U +
                          (uint8_t)(s_cursor + 1U));
    s_data_count++;
    return true;
}

bool I2C_AVR_Read(uint8_t *value, bool ack)
{
    (void)ack;
    if (!s_reading || value == NULL) return false;
    *value = s_memory[s_cursor];
    s_cursor = (uint16_t)((uint16_t)s_block * 256U +
                          (uint8_t)(s_cursor + 1U));
    return true;
}

void I2C_AVR_Stop(void)
{
    if (s_data_count != 0U)
    {
        if (s_page_count < 40U) s_page_lengths[s_page_count] = s_data_count;
        s_page_count++;
        s_busy_count = 2U;
    }
    s_data_count = 0U;
}

bool BSP_I2C_Probe(uint8_t address)
{
    s_probe_count++;
    if (address != 0x50U && address != 0x51U) return false;
    if (s_never_ready) return false;
    if (s_busy_count != 0U)
    {
        s_busy_count--;
        return false;
    }
    return true;
}

int main(void)
{
    uint8_t data[32];
    uint8_t result[32];
    unsigned int i;

    memset(s_memory, 0xFF, sizeof(s_memory));
    MCUCR = _BV(SRE);
    BSP_EEPROM_Init();
    CHECK((DDRA & _BV(PA0)) != 0U);
    CHECK((PORTA & _BV(PA0)) != 0U);
    CHECK((MCUCR & _BV(SRE)) == 0U);
    CHECK(BSP_EEPROM_Probe());

    for (i = 0U; i < sizeof(data); i++) data[i] = (uint8_t)(i + 1U);
    CHECK(BSP_EEPROM_Write(250U, data, sizeof(data)));
    CHECK(s_wp_low_for_data);
    CHECK((PORTA & _BV(PA0)) != 0U);
    CHECK(s_page_count == 3U);
    CHECK(s_page_addresses[0] == 250U && s_page_lengths[0] == 6U);
    CHECK(s_page_addresses[1] == 256U && s_page_lengths[1] == 16U);
    CHECK(s_page_addresses[2] == 272U && s_page_lengths[2] == 10U);
    CHECK(s_probe_count >= 10U); /* ACK polling followed each page write. */
    memset(result, 0, sizeof(result));
    CHECK(BSP_EEPROM_Read(250U, result, sizeof(result)));
    CHECK(memcmp(result, data, sizeof(data)) == 0);
    CHECK(s_memory[249] == 0xFFU && s_memory[282] == 0xFFU);

    CHECK(BSP_EEPROM_Write(510U, data, 2U));
    CHECK(s_memory[510] == data[0] && s_memory[511] == data[1]);
    CHECK((PORTA & _BV(PA0)) != 0U);

    CHECK(!BSP_EEPROM_Read(511U, result, 2U));
    CHECK(!BSP_EEPROM_Write(511U, data, 2U));
    CHECK(!BSP_EEPROM_Write(513U, data, 1U));
    CHECK(!BSP_EEPROM_Write(0U, NULL, 1U));
    CHECK(BSP_EEPROM_Write(512U, NULL, 0U));
    CHECK((PORTA & _BV(PA0)) != 0U);

    s_ignore_writes = true;
    CHECK(!BSP_EEPROM_Write(0U, data, 1U)); /* ACK with unchanged data. */
    CHECK((PORTA & _BV(PA0)) != 0U);
    s_ignore_writes = false;

    s_fail_data = true;
    CHECK(!BSP_EEPROM_Write(1U, data, 1U));
    CHECK((PORTA & _BV(PA0)) != 0U);
    s_fail_data = false;

    s_never_ready = true;
    CHECK(!BSP_EEPROM_Write(2U, data, 1U));
    CHECK((PORTA & _BV(PA0)) != 0U);

    if (s_failures != 0U)
    {
        fprintf(stderr, "%u EEPROM checks failed\n", s_failures);
        return 1;
    }
    puts("24C04 EEPROM tests passed");
    return 0;
}
