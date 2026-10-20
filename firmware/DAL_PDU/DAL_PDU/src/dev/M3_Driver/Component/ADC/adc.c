#include "adc.h"

#include <avr/io.h>

#define ADC_CHANNEL_COUNT          8U
#define ADC_PRESCALE_128           (_BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0))
#define ADC_CONVERSION_POLL_LIMIT  2000U

void ADC_Init(void)
{
    ADMUX = _BV(REFS0);
    ADCSRA = _BV(ADEN) | ADC_PRESCALE_128;
}

bool ADC_Read(uint8_t channel, uint16_t *counts)
{
    uint16_t remaining = ADC_CONVERSION_POLL_LIMIT;

    if (counts == 0 || channel >= ADC_CHANNEL_COUNT) return false;

    ADMUX = (uint8_t)((ADMUX & (_BV(REFS0) | _BV(ADLAR))) | channel);
    ADCSRA |= _BV(ADSC);
    while ((ADCSRA & _BV(ADSC)) != 0U)
    {
        if (--remaining == 0U) return false;
    }
    *counts = ADC;
    return true;
}
