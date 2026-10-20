#ifndef ADC_H_
#define ADC_H_

#include <stdbool.h>
#include <stdint.h>

/* Enables the ADC with the AVcc reference and a 93.75 kHz clock (/128) at 12 MHz. */
void ADC_Init(void);

/* One blocking, single-ended conversion on channel 0-7. Returns false for an
 * invalid channel, a null pointer, or a conversion that does not finish in time. */
bool ADC_Read(uint8_t channel, uint16_t *counts);

#endif /* ADC_H_ */
