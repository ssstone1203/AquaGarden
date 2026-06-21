#include "soil_moisture_adc.h"

enum
{
    SOIL_ADC_CALIBRATION_TIMEOUT_MS = 1000U
};

volatile uint16_t g_soil_adc_cal_dry_raw = SOIL_ADC_DRY_CAL_RAW;
volatile uint16_t g_soil_adc_cal_wet_raw = SOIL_ADC_WET_CAL_RAW;

static fsp_err_t soil_adc_wait_for_idle(void)
{
    adc_status_t status = {0};
    uint32_t     timeout_ms = SOIL_ADC_CALIBRATION_TIMEOUT_MS;

    while (timeout_ms > 0U)
    {
        fsp_err_t err = R_ADC_B_StatusGet(&g_adc_b_ctrl, &status);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        if (ADC_STATE_IDLE == status.state)
        {
            return FSP_SUCCESS;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        timeout_ms--;
    }

    return FSP_ERR_TIMEOUT;
}

void soil_adc_init(void)
{
    fsp_err_t err;

    err = R_ADC_B_Open(&g_adc_b_ctrl, &g_adc_b_cfg);
    if (FSP_SUCCESS != err)
    {
        return;
    }

    err = R_ADC_B_ScanCfg(&g_adc_b_ctrl, &g_adc_b_scan_cfg);
    if (FSP_SUCCESS != err)
    {
        return;
    }

    err = R_ADC_B_Calibrate(&g_adc_b_ctrl, NULL);
    if (FSP_SUCCESS != err)
    {
        return;
    }

    (void) soil_adc_wait_for_idle();
}

uint16_t soil_adc_read_raw(void)
{
    fsp_err_t  err;
    uint16_t   raw_value = SOIL_ADC_MAX_VALUE;

    err = R_ADC_B_ScanStart(&g_adc_b_ctrl);
    if (FSP_SUCCESS != err)
    {
        return SOIL_ADC_MAX_VALUE;
    }

    err = soil_adc_wait_for_idle();
    if (FSP_SUCCESS != err)
    {
        return SOIL_ADC_MAX_VALUE;
    }

    err = R_ADC_B_Read(&g_adc_b_ctrl, SOIL_ADC_CHANNEL, &raw_value);
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
