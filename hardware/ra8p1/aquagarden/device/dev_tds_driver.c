#include "dev_tds_driver.h"
#include "dev_adc_shared.h"
#include "../ra_gen/main_service.h"

#define TDS_ADC_CHANNEL     ADC_CHANNEL_2
#define TDS_ADC_MAX_RAW     (4095.0F)
#define TDS_ADC_VREF        (3.0F)
#define TDS_FILTER_COUNT    (5U)

static uint16_t s_samples[TDS_FILTER_COUNT];
static uint8_t  s_sample_count;
static uint8_t  s_sample_index;

static uint16_t tds_median(uint16_t * p_values, uint8_t count)
{
    for (uint8_t i = 0U; i < count; i++)
    {
        for (uint8_t j = (uint8_t) (i + 1U); j < count; j++)
        {
            if (p_values[j] < p_values[i])
            {
                uint16_t tmp = p_values[i];
                p_values[i] = p_values[j];
                p_values[j] = tmp;
            }
        }
    }

    return p_values[count / 2U];
}

static uint16_t tds_voltage_to_ntu(float voltage)
{
    float ntu;

    if (voltage < 0.0F)
    {
        voltage = 0.0F;
    }
    if (voltage > 3.0F)
    {
        voltage = 3.0F;
    }

    ntu = (66.71F * voltage * voltage * voltage) -
          (127.93F * voltage * voltage) +
          (428.7F * voltage);

    if (ntu < 0.0F)
    {
        return 0U;
    }
    if (ntu > 65535.0F)
    {
        return 65535U;
    }

    return (uint16_t) (ntu + 0.5F);
}

void dev_tds_init(void)
{
    dev_adc_shared_init();
}

fsp_err_t dev_tds_read(uint16_t * p_raw, float * p_voltage, uint16_t * p_ntu)
{
    uint16_t raw;
    uint16_t filtered;
    uint16_t temp[TDS_FILTER_COUNT];
    fsp_err_t err;

    if ((NULL == p_raw) || (NULL == p_voltage) || (NULL == p_ntu))
    {
        return FSP_ERR_INVALID_POINTER;
    }

    dev_adc_shared_init();
    err = g_adc0.p_api->read(g_adc0.p_ctrl, TDS_ADC_CHANNEL, &raw);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    s_samples[s_sample_index] = raw;
    s_sample_index = (uint8_t) ((s_sample_index + 1U) % TDS_FILTER_COUNT);
    if (s_sample_count < TDS_FILTER_COUNT)
    {
        s_sample_count++;
    }

    for (uint8_t i = 0U; i < s_sample_count; i++)
    {
        temp[i] = s_samples[i];
    }

    filtered = tds_median(temp, s_sample_count);
    *p_raw = filtered;
    *p_voltage = (float) filtered * TDS_ADC_VREF / TDS_ADC_MAX_RAW;
    *p_ntu = tds_voltage_to_ntu(*p_voltage);

    return FSP_SUCCESS;
}
