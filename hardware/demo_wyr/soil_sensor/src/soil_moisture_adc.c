#include "soil_moisture_adc.h"
#include "bsp_api.h"
//#include "bsp_module_stop.h"

static uint16_t soil_adc_channel_mask_low(uint32_t channel)
{
    if (channel < 16U)
    {
        return (uint16_t) (1UL << channel);
    }

    return 0U;
}

static uint16_t soil_adc_channel_mask_high(uint32_t channel)
{
    if ((channel >= 16U) && (channel < 32U))
    {
        return (uint16_t) (1UL << (channel - 16U));
    }

    return 0U;
}

void soil_adc_init(void)
{
    /* Enable module clock for ADC0. */
    R_BSP_MODULE_START(FSP_IP_ADC, 0);

    /* Stop conversion first. */
    R_ADC0->ADCSR_b.ADST = 0U;

    /* Disable external trigger, use software trigger, single scan. */
    R_ADC0->ADCSR_b.TRGE = 0U;
    R_ADC0->ADCSR_b.EXTRG = 0U;
    R_ADC0->ADCSR_b.ADCS = 0U;
    R_ADC0->ADCSR_b.ADIE = 0U;

    /* 12-bit, right aligned output. */
    R_ADC0->ADCER_b.ADPRC = 0U;
    R_ADC0->ADCER_b.ADRFMT = 0U;

    /* Select only one channel for group A. */
    R_ADC0->ADANSA[0] = soil_adc_channel_mask_low(SOIL_ADC_CHANNEL_INDEX);
    R_ADC0->ADANSA[1] = soil_adc_channel_mask_high(SOIL_ADC_CHANNEL_INDEX);

    /* Disable average/addition by default. */
    R_ADC0->ADADC = 0U;
}

uint16_t soil_adc_read_raw(void)
{
    uint32_t timeout = 1000000U;
    uint16_t raw = 0U;

    R_ADC0->ADCSR_b.ADST = 1U;

    while ((R_ADC0->ADCSR_b.ADST != 0U) && (timeout > 0U))
    {
        timeout--;
    }

    raw = (uint16_t) (R_ADC0->ADDR[SOIL_ADC_CHANNEL_INDEX] & SOIL_ADC_MAX_VALUE);
    return raw;
}

uint8_t soil_adc_raw_to_percent(uint16_t raw_value)
{
    int32_t dry = (int32_t) SOIL_ADC_DRY_CAL_RAW;
    int32_t wet = (int32_t) SOIL_ADC_WET_CAL_RAW;
    int32_t value = (int32_t) raw_value;
    int32_t percent = 0;

    if (dry == wet)
    {
        return 0U;
    }

    /* Force output rule: higher moisture => higher percentage. */
    if (wet > dry)
    {
        percent = ((value - dry) * 100) / (wet - dry);
    }
    else
    {
        percent = ((dry - value) * 100) / (dry - wet);
    }

    if (percent < 0)
    {
        percent = 0;
    }
    else if (percent > 100)
    {
        percent = 100;
    }

    return (uint8_t) percent;
}
