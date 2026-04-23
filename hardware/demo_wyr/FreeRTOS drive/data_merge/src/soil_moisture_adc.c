#include "soil_moisture_adc.h"

/* This project currently has no FSP ADC stack instance generated.
 * Use ADC0 register-level sampling so soil value can update in realtime.
 */
enum
{
    SOIL_ADC_UNIT                = 0U,
    SOIL_ADC_TIMEOUT_LOOP_COUNT  = 100000U
};

volatile uint16_t g_soil_adc_cal_dry_raw = SOIL_ADC_DRY_CAL_RAW;
volatile uint16_t g_soil_adc_cal_wet_raw = SOIL_ADC_WET_CAL_RAW;

void soil_adc_init(void)
{
    uint16_t channel_mask;

    /* Release ADC0 from module-stop state. */
    R_BSP_MODULE_START(FSP_IP_ADC, SOIL_ADC_UNIT);

    /* Ensure conversion is stopped before changing configuration bits. */
    R_ADC0->ADCSR_b.ADST = 0U;

    /* Software trigger mode (TRGE=0), single scan, no interrupts. */
    R_ADC0->ADCSR = 0U;

    /* Select one channel in Group A scan list. */
    channel_mask = (uint16_t) (1UL << SOIL_ADC_CHANNEL_INDEX);
    R_ADC0->ADANSA[0] = channel_mask;
    R_ADC0->ADANSA[1] = 0U;

    /* 12-bit mode, right-justified result (default style). */
    R_ADC0->ADCER = 0U;
}

uint16_t soil_adc_read_raw(void)
{
    uint32_t timeout = SOIL_ADC_TIMEOUT_LOOP_COUNT;

    /* Start one software-trigger conversion. */
    R_ADC0->ADCSR_b.ADST = 1U;

    /* Wait until hardware clears ADST at end of conversion. */
    while ((0U != R_ADC0->ADCSR_b.ADST) && (timeout > 0U))
    {
        timeout--;
    }

    if (0U == timeout)
    {
        /* Propagate an out-of-range value so upper layer can flag timeout/suspect state. */
        return SOIL_ADC_MAX_VALUE;
    }

    return (uint16_t) (R_ADC0->ADDR[SOIL_ADC_CHANNEL_INDEX] & SOIL_ADC_MAX_VALUE);
}

uint8_t soil_adc_raw_to_percent(uint16_t raw_value)
{
    uint16_t clamped_raw = raw_value;
    uint32_t numerator;

    /* MD0504 datasheet: moisture increases -> analog value decreases.
     * Use full ADC range linear inverse mapping:
     *   raw=0    -> 100%
     *   raw=4095 -> 0%
     */
    if (clamped_raw > SOIL_ADC_MAX_VALUE)
    {
        clamped_raw = SOIL_ADC_MAX_VALUE;
    }

    numerator = ((uint32_t) (SOIL_ADC_MAX_VALUE - clamped_raw) * 100U) + (SOIL_ADC_MAX_VALUE / 2U);
    return (uint8_t) (numerator / SOIL_ADC_MAX_VALUE);
}
