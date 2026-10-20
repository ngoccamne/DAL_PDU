#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M3_Driver/Component/DIO/do.h"

volatile uint8_t DDRA, PORTA, DDRB, PORTB, DDRC, PORTC, DDRD, PORTD;
volatile uint8_t DDRE, PORTE, DDRF, PORTF, DDRG, PORTG;

static unsigned int failures;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expression); \
        failures++; \
    } \
} while (0)

static void test_port_a_output(void)
{
    do_t led = { DO_PORT_A, 3U, false };

    DDRA = 0x02U;
    PORTA = 0x0AU;
    CHECK(do_init(&led));
    CHECK(DDRA == 0x0AU);
    CHECK(PORTA == 0x02U);
    CHECK(!led.bStatus);

    do_set(&led);
    CHECK(PORTA == 0x0AU);
    CHECK(led.bStatus);
    do_toggle(&led);
    CHECK(PORTA == 0x02U);
    CHECK(!led.bStatus);
    do_toggle(&led);
    CHECK(PORTA == 0x0AU);
    CHECK(led.bStatus);
    do_reset(&led);
    CHECK(PORTA == 0x02U);
    CHECK(!led.bStatus);
}

static void test_other_ports_and_invalid_pins(void)
{
    do_t port_b = { DO_PORT_B, 7U, true };
    do_t port_g = { DO_PORT_G, 4U, true };
    do_t invalid = { DO_PORT_G, 5U, false };

    CHECK(do_init(&port_b));
    CHECK((DDRB & 0x80U) != 0U);
    CHECK((PORTB & 0x80U) != 0U);
    CHECK(do_init(&port_g));
    CHECK((DDRG & 0x10U) != 0U);
    CHECK((PORTG & 0x10U) != 0U);

    CHECK(!do_init(&invalid));
    do_set(&invalid);
    CHECK(!invalid.bStatus);
    CHECK(PORTG == 0x10U);
    invalid.port = 0U;
    invalid.pin = 0U;
    CHECK(!do_init(&invalid));
    invalid.port = DO_PORT_A;
    invalid.pin = 8U;
    CHECK(!do_init(&invalid));
    CHECK(!do_init(NULL));
    do_toggle(NULL);
}

int main(void)
{
    test_port_a_output();
    test_other_ports_and_invalid_pins();

    if (failures != 0U)
    {
        fprintf(stderr, "%u DIO checks failed\n", failures);
        return 1;
    }

    puts("DIO tests passed");
    return 0;
}
