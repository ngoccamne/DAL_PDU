#include "bsp_ntc.h"

#include "../dal_pdu_board.h"
#include "../../M3_Driver/Component/ADC/adc.h"

/* AVcc is assumed to be 5.000 V. Each 1% supply error shifts the reading by
 * about 0.75 C at 25 C. */
#define NTC_AVCC_MV          5000UL
#define NTC_ADC_FULL_SCALE   1024UL
#define NTC_SAMPLES          16U
#define NTC_OFFSET_MV        500L
#define NTC_MIN_TENTHS_C     (-400L)
#define NTC_MAX_TENTHS_C     1250L

bool BSP_NTC_Init(void)
{
    uint16_t discarded;
    ADC_Init();
    return ADC_Read(TEMP_ADC_CHANNEL, &discarded);
}

bool BSP_NTC_ConvertTenthsC(uint32_t count_sum, uint8_t samples, int16_t *tenths_c)
{
    uint32_t millivolts;
    int32_t tenths;

    if (tenths_c == 0 || samples == 0U) return false;

    millivolts = (count_sum * NTC_AVCC_MV) / (NTC_ADC_FULL_SCALE * samples);
    tenths = (int32_t)millivolts - NTC_OFFSET_MV;
    if (tenths < NTC_MIN_TENTHS_C || tenths > NTC_MAX_TENTHS_C) return false;

    *tenths_c = (int16_t)tenths;
    return true;
}

bool BSP_NTC_ReadTenthsC(int16_t *tenths_c)
{
    uint32_t sum = 0U;
    uint8_t index;
    uint16_t counts;

    if (tenths_c == 0) return false;

    for (index = 0U; index < NTC_SAMPLES; index++)
    {
        if (!ADC_Read(TEMP_ADC_CHANNEL, &counts)) return false;
        sum += counts;
    }
    return BSP_NTC_ConvertTenthsC(sum, NTC_SAMPLES, tenths_c);
}
