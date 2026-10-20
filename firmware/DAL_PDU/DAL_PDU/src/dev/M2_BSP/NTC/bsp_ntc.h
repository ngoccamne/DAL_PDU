#ifndef BSP_NTC_H_
#define BSP_NTC_H_

#include <stdbool.h>
#include <stdint.h>

/* Board temperature from the TC1047A on the TEMP net (PF1/ADC1). The sensor is
 * linear, not an NTC; the module keeps the name it was requested under. */
bool BSP_NTC_Init(void);

/* Averages 16 conversions. Returns false if the ADC fails or the reading is
 * outside the TC1047A range of -40 C to +125 C. */
bool BSP_NTC_ReadTenthsC(int16_t *tenths_c);

/* Converts the sum of `samples` ADC counts to tenths of a degree C. Returns
 * false when the result is outside -40 C to +125 C. */
bool BSP_NTC_ConvertTenthsC(uint32_t count_sum, uint8_t samples, int16_t *tenths_c);

#endif /* BSP_NTC_H_ */
