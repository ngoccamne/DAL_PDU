#include <stdio.h>
#include <stdint.h>

#include "fifo.h"

static unsigned int failures;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expression); \
        failures++; \
    } \
} while (0)

static void test_invalid_setup_and_arguments(void)
{
    FIFO_t fifo;
    uint8_t storage[2];
    uint8_t value = 0xA5U;

    CHECK(FIFO_Init(NULL, storage, sizeof(storage)) == false);
    CHECK(FIFO_Init(&fifo, NULL, sizeof(storage)) == false);
    CHECK(FIFO_Count(&fifo) == 0U);
    CHECK(FIFO_Push(&fifo, 1U) == false);
    CHECK(FIFO_Pop(&fifo, &value) == false);
    CHECK(value == 0xA5U);

    CHECK(FIFO_Init(&fifo, storage, 0U) == false);
    CHECK(FIFO_Init(&fifo, storage, sizeof(storage)) == true);
    CHECK(fifo.buffer == storage);
    CHECK(FIFO_Pop(&fifo, NULL) == false);
    CHECK(FIFO_Push(NULL, 1U) == false);
    CHECK(FIFO_Pop(NULL, &value) == false);
    CHECK(FIFO_Count(NULL) == 0U);
    FIFO_Clear(NULL);
}

static void test_full_empty_and_wraparound(void)
{
    FIFO_t fifo;
    uint8_t storage[3];
    uint8_t value = 0xA5U;

    CHECK(FIFO_Init(&fifo, storage, sizeof(storage)) == true);
    CHECK(FIFO_Pop(&fifo, &value) == false);
    CHECK(value == 0xA5U);
    CHECK(FIFO_Push(&fifo, 1U) == true);
    CHECK(FIFO_Push(&fifo, 2U) == true);
    CHECK(FIFO_Push(&fifo, 3U) == true);
    CHECK(FIFO_Count(&fifo) == 3U);
    CHECK(FIFO_Push(&fifo, 99U) == false);
    CHECK(FIFO_Count(&fifo) == 3U);

    CHECK(FIFO_Pop(&fifo, &value) == true);
    CHECK(value == 1U);
    CHECK(FIFO_Push(&fifo, 4U) == true);
    CHECK(FIFO_Pop(&fifo, &value) == true);
    CHECK(value == 2U);
    CHECK(FIFO_Pop(&fifo, &value) == true);
    CHECK(value == 3U);
    CHECK(FIFO_Pop(&fifo, &value) == true);
    CHECK(value == 4U);
    CHECK(FIFO_Pop(&fifo, &value) == false);
    CHECK(value == 4U);
    CHECK(FIFO_Count(&fifo) == 0U);
}

static void test_single_byte_clear_and_separate_instances(void)
{
    FIFO_t rx;
    FIFO_t tx;
    uint8_t rx_storage[1];
    uint8_t tx_storage[4];
    uint8_t value;

    CHECK(FIFO_Init(&rx, rx_storage, sizeof(rx_storage)) == true);
    CHECK(FIFO_Init(&tx, tx_storage, sizeof(tx_storage)) == true);
    CHECK(FIFO_Push(&rx, 10U) == true);
    CHECK(FIFO_Push(&rx, 11U) == false);
    CHECK(FIFO_Push(&tx, 20U) == true);
    FIFO_Clear(&rx);
    CHECK(FIFO_Count(&rx) == 0U);
    CHECK(FIFO_Count(&tx) == 1U);
    CHECK(FIFO_Push(&rx, 12U) == true);
    CHECK(FIFO_Pop(&rx, &value) == true);
    CHECK(value == 12U);
    CHECK(FIFO_Pop(&tx, &value) == true);
    CHECK(value == 20U);
}

static void test_capacity_above_255_bytes(void)
{
    FIFO_t fifo;
    uint8_t storage[300];
    uint8_t value;
    uint16_t i;

    CHECK(FIFO_Init(&fifo, storage, sizeof(storage)) == true);
    for (i = 0U; i < 300U; i++)
    {
        CHECK(FIFO_Push(&fifo, (uint8_t)i) == true);
    }
    CHECK(FIFO_Count(&fifo) == 300U);

    for (i = 0U; i < 150U; i++)
    {
        CHECK(FIFO_Pop(&fifo, &value) == true);
        CHECK(value == (uint8_t)i);
    }
    for (i = 300U; i < 450U; i++)
    {
        CHECK(FIFO_Push(&fifo, (uint8_t)i) == true);
    }
    CHECK(FIFO_Count(&fifo) == 300U);
    for (i = 150U; i < 450U; i++)
    {
        CHECK(FIFO_Pop(&fifo, &value) == true);
        CHECK(value == (uint8_t)i);
    }
    CHECK(FIFO_Count(&fifo) == 0U);
}

int main(void)
{
    test_invalid_setup_and_arguments();
    test_full_empty_and_wraparound();
    test_single_byte_clear_and_separate_instances();
    test_capacity_above_255_bytes();

    if (failures != 0U)
    {
        fprintf(stderr, "%u FIFO checks failed\n", failures);
        return 1;
    }

    puts("FIFO tests passed");
    return 0;
}
