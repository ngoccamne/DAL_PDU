#ifndef FIFO_H_
#define FIFO_H_

#include <stdbool.h>
#include <stdint.h>

/* Caller owns the storage for the entire lifetime of the FIFO. Initialize
 * before enabling interrupts; Push, Pop, Count, and Clear are atomic on AVR. */
typedef struct
{
    uint8_t *buffer;
    uint16_t capacity;
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} FIFO_t;

/* Returns false for a null pointer or zero capacity. An invalid initialization
 * leaves a non-null FIFO empty and unusable until initialized successfully. */
bool FIFO_Init(FIFO_t *fifo, uint8_t *buffer, uint16_t capacity);

/* Nonblocking: Push returns false when full; Pop returns false when empty.
 * A failed Push leaves the queue unchanged; a failed Pop leaves *value unchanged. */
bool FIFO_Push(FIFO_t *fifo, uint8_t value);
bool FIFO_Pop(FIFO_t *fifo, uint8_t *value);

uint16_t FIFO_Count(const FIFO_t *fifo);
void FIFO_Clear(FIFO_t *fifo);

#endif /* FIFO_H_ */
