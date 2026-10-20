#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/I2C/bsp_i2c.h"

/* Port registers used by bsp_i2c.c for the TWI pins. */
volatile uint8_t DDRD, PORTD;

static unsigned failures;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

/* Simulated TWI results: 0 acknowledged, 1 NACK, 2 timed out. */
static unsigned fail_mode;
static bool timed_out;
static unsigned recover_calls;
static uint8_t read_value = 0x5AU;

static bool TwiResult(void)
{
    if (fail_mode == 2U) timed_out = true;
    return fail_mode == 0U;
}

bool I2C_AVR_Init(uint32_t cpu_hz, uint32_t bus_hz)
{
    (void)cpu_hz;
    (void)bus_hz;
    return true;
}

bool I2C_AVR_Start(uint8_t address_rw)
{
    (void)address_rw;
    return TwiResult();
}

bool I2C_AVR_Write(uint8_t value)
{
    (void)value;
    return TwiResult();
}

bool I2C_AVR_Read(uint8_t *value, bool ack)
{
    (void)ack;
    if (fail_mode == 0U) *value = read_value;
    return TwiResult();
}

void I2C_AVR_Stop(void)
{
}

void I2C_AVR_ClearTimeout(void)
{
    timed_out = false;
}

bool I2C_AVR_TimedOut(void)
{
    return timed_out;
}

void I2C_AVR_Recover(void)
{
    recover_calls++;
}

static void test_acknowledged_transaction_clears_fault(void)
{
    uint8_t value = 0U;
    fail_mode = 2U;
    CHECK(!BSP_I2C_ReadRegister(0x1FU, 0x01U, &value));
    CHECK(BSP_I2C_BusFault());

    fail_mode = 0U;
    CHECK(BSP_I2C_ReadRegister(0x1FU, 0x01U, &value));
    CHECK(value == 0x5AU);
    CHECK(!BSP_I2C_BusFault());
}

static void test_timeout_sets_fault_and_recovers_bus(void)
{
    unsigned before;
    uint8_t value = 0U;
    fail_mode = 0U;
    CHECK(BSP_I2C_ReadRegister(0x1FU, 0x01U, &value));
    before = recover_calls;

    fail_mode = 2U;
    CHECK(!BSP_I2C_ReadRegister(0x1FU, 0x01U, &value));
    CHECK(BSP_I2C_BusFault());
    CHECK(BSP_I2C_FaultAddress() == 0x1FU);
    CHECK(recover_calls == before + 1U);
}

static void test_nack_is_not_a_bus_fault(void)
{
    unsigned before;
    fail_mode = 0U;
    CHECK(BSP_I2C_SendCommand(0x1FU, 0x00U));
    CHECK(!BSP_I2C_BusFault());

    before = recover_calls;
    fail_mode = 1U;
    CHECK(!BSP_I2C_Probe(0x50U));
    CHECK(!BSP_I2C_BusFault());
    CHECK(recover_calls == before);
}

static void test_nack_keeps_an_existing_fault(void)
{
    fail_mode = 2U;
    CHECK(!BSP_I2C_Probe(0x74U));
    CHECK(BSP_I2C_BusFault());

    fail_mode = 1U;
    CHECK(!BSP_I2C_Probe(0x50U));
    CHECK(BSP_I2C_BusFault());

    fail_mode = 0U;
    CHECK(BSP_I2C_Probe(0x74U));
    CHECK(!BSP_I2C_BusFault());
}

int main(void)
{
    test_acknowledged_transaction_clears_fault();
    test_timeout_sets_fault_and_recovers_bus();
    test_nack_is_not_a_bus_fault();
    test_nack_keeps_an_existing_fault();
    if (failures != 0U)
    {
        fprintf(stderr, "%u I2C bus checks failed\n", failures);
        return 1;
    }
    puts("I2C bus tests passed");
    return 0;
}
