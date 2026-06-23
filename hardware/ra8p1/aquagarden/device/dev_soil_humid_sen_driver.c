#include "dev_soil_humid_sen_driver.h"
#include "dev_adc_shared.h"
#include "../ra_gen/main_service.h"

#define SOIL_ADC_CHANNEL     ADC_CHANNEL_1
#define SOIL_ADC_DRY_RAW     (3200U)
#define SOIL_ADC_WET_RAW     (1500U)

static uint8_t soil_raw_to_percent(uint16_t raw)
{
    int32_t percent;

    if (raw >= SOIL_ADC_DRY_RAW)
    {
        return 0U;
    }
    if (raw <= SOIL_ADC_WET_RAW)
    {
        return 100U;
    }

    percent = ((int32_t) SOIL_ADC_DRY_RAW - (int32_t) raw) * 100L /
              ((int32_t) SOIL_ADC_DRY_RAW - (int32_t) SOIL_ADC_WET_RAW);

    if (percent < 0)
    {
        return 0U;
    }
    if (percent > 100)
    {
        return 100U;
    }

    return (uint8_t) percent;
}

void dev_soil_init(void)
{
    dev_adc_shared_init();
}

fsp_err_t dev_soil_read(uint16_t * p_raw, uint8_t * p_percent)
{
    uint16_t raw = 0U;
    fsp_err_t err;

    if ((NULL == p_raw) || (NULL == p_percent))
    {
        return FSP_ERR_INVALID_POINTER;
    }

    dev_adc_shared_init();
    err = g_adc0.p_api->read(g_adc0.p_ctrl, SOIL_ADC_CHANNEL, &raw);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    *p_raw = raw;
    *p_percent = soil_raw_to_percent(raw);
    return FSP_SUCCESS;
}
