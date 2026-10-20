#ifndef DO_H_
#define DO_H_

#include <stdbool.h>
#include <stdint.h>

/* Port numbers follow the chapter 9 DIO interface: A = 1, B = 2, etc. */
typedef enum
{
    DO_PORT_A = 1,
    DO_PORT_B,
    DO_PORT_C,
    DO_PORT_D,
    DO_PORT_E,
    DO_PORT_F,
    DO_PORT_G
} do_port_t;

typedef struct
{
    uint32_t port;
    uint32_t pin;
    bool bStatus;
} do_t;

/* AVR extension: set the output latch before switching the pin to output. */
bool do_init(do_t *me);

void do_set(do_t *me);
void do_reset(do_t *me);
void do_toggle(do_t *me);

#endif /* DO_H_ */
