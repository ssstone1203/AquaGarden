#include "dev_soil_humid_sen_driver.h"
#include "dev_adc_shared.h"
#include "../ra_gen/main_service.h"

/* hardware/demo/soil_sensor: continuous scan, read ADC_CHANNEL_1. */
#define SOIL_ADC_CHANNEL  ADC_CHANNEL_1
#define SOIL_ADC_MAX_RAW  (4095U)

/* Protocol needs 0..100 %; demo only exposes raw. Use inverse linear map
 * (wetter -> lower ADC), same as aquagarden / MD0504 practice. */
static uint8_t soil_raw_to_percent(uint16_t raw)
{
    uint16_t clamped = (raw > SOIL_ADC_MAX_RAW) ? SOIL_ADC_MAX_RAW : raw;
    uint32_t numerator = ((uint32_t) (SOIL_ADC_MAX_RAW - clamped) * 100U) +
                         (SOIL_ADC_MAX_RAW / 2U);

    return (uint8_t) (numerator / SOIL_ADC_MAX_RAW);
}

void dev_soil_init(void)
{
    /* demo: SHS_Init() + SHS_ScanStart() */
    dev_adc_shared_init();
}

fsp_err_t dev_soil_read(uint16_t * p_raw, uint8_t * p_percent)
{
    uint16_t  raw = 0U;
    fsp_err_t err;

    if ((NULL == p_raw) || (NULL == p_percent))
    {
        return FSP_ERR_INVALID_POINTER;
    }

    /* demo: SHS_ScanCpltCallback -> R_ADC_Read(..., ADC_CHANNEL_1, ...) */
    dev_adc_shared_init();
    if (!dev_adc_shared_is_ready())
    {
        return FSP_ERR_NOT_INITIALIZED;
    }

    err = g_adc0.p_api->read(g_adc0.p_ctrl, SOIL_ADC_CHANNEL, &raw);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    *p_raw = raw;
    *p_percent = soil_raw_to_percent(raw);
    return FSP_SUCCESS;
}
