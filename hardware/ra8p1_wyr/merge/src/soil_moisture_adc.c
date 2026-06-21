#include "soil_moisture_adc.h"
#include "adc_shared.h"

volatile uint16_t g_soil_adc_cal_dry_raw = SOIL_ADC_DRY_CAL_RAW;
volatile uint16_t g_soil_adc_cal_wet_raw = SOIL_ADC_WET_CAL_RAW;

void soil_adc_init(void)
{
    adc_shared_init();
}

uint16_t soil_adc_read_raw(void)
{
    uint16_t  raw_value = SOIL_ADC_MAX_VALUE;
    fsp_err_t err       = adc_shared_read(SOIL_ADC_CHANNEL, &raw_value);

    if (FSP_SUCCESS != err)
    {
        return SOIL_ADC_MAX_VALUE;
    }

    return raw_value;
}

uint8_t soil_adc_raw_to_percent(uint16_t raw_value)
{
    uint16_t clamped_raw = raw_value;
    uint32_t numerator;

    /* MD0504: moisture increases -> analog value decreases. */
    if (clamped_raw > SOIL_ADC_MAX_VALUE)
    {
        clamped_raw = SOIL_ADC_MAX_VALUE;
    }

    numerator = ((uint32_t) (SOIL_ADC_MAX_VALUE - clamped_raw) * 100U) + (SOIL_ADC_MAX_VALUE / 2U);
    return (uint8_t) (numerator / SOIL_ADC_MAX_VALUE);
}
