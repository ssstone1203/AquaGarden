#include "dev_tds_driver.h"
#include "adc_shared.h"
#include "sensor_fusion.h"

#include "FreeRTOS.h"
#include "task.h"

/* Keil Watch window variables (global + volatile for realtime debug). */
volatile uint16_t g_tds_adc_raw           = 0U;
volatile uint16_t g_tds_adc_median_raw    = 0U;
volatile float    g_tds_voltage_v         = 0.0f;
volatile float    g_tds_value_ppm         = 0.0f;
volatile uint32_t g_tds_sample_count      = 0U;
volatile uint8_t  g_tds_sensor_state      = 0U; /* 0: normal, 1: adc timeout/suspect */
volatile float    g_tds_water_temp_c      = 25.0f;

static uint16_t   s_analog_buffer[TDS_MEDIAN_FILTER_LEN];
static uint16_t   s_median_scratch[TDS_MEDIAN_FILTER_LEN];
static uint16_t   s_analog_buffer_index   = 0U;
static TickType_t s_last_sample_tick      = 0U;
static TickType_t s_last_update_tick      = 0U;

static uint16_t tds_median_filter(uint16_t const * samples, uint16_t length)
{
    uint16_t i;
    uint16_t j;

    for (i = 0U; i < length; i++)
    {
        s_median_scratch[i] = samples[i];
    }

    for (j = 0U; j < (length - 1U); j++)
    {
        for (i = 0U; i < ((length - j) - 1U); i++)
        {
            if (s_median_scratch[i] > s_median_scratch[i + 1U])
            {
                uint16_t temp              = s_median_scratch[i];
                s_median_scratch[i]        = s_median_scratch[i + 1U];
                s_median_scratch[i + 1U]   = temp;
            }
        }
    }

    if ((length & 1U) > 0U)
    {
        return s_median_scratch[(length - 1U) / 2U];
    }

    return (uint16_t) (((uint32_t) s_median_scratch[length / 2U] +
                        (uint32_t) s_median_scratch[(length / 2U) - 1U]) / 2U);
}

static float tds_voltage_to_ppm(float voltage_v, float water_temp_c)
{
    float compensation_coefficient;
    float compensation_voltage;
    float tds_ppm;

    /* Vendor formula from TDS module Arduino example. */
    compensation_coefficient = 1.0f + (0.02f * (water_temp_c - 25.0f));
    compensation_voltage     = voltage_v / compensation_coefficient;
    tds_ppm = ((133.42f * compensation_voltage * compensation_voltage * compensation_voltage) -
               (255.86f * compensation_voltage * compensation_voltage) +
               (857.39f * compensation_voltage)) * 0.5f;

    if (tds_ppm < 0.0f)
    {
        tds_ppm = 0.0f;
    }

    return tds_ppm;
}

static uint8_t tds_adc_read_is_valid(uint16_t raw_value, fsp_err_t err)
{
    if ((FSP_SUCCESS != err) && (FSP_ERR_INVALID_DATA != err))
    {
        return 0U;
    }

    if (raw_value >= TDS_ADC_MAX_VALUE)
    {
        return 0U;
    }

    return 1U;
}

void tds_adc_init(void)
{
    adc_shared_init();
}

uint16_t tds_adc_read_raw(void)
{
    uint16_t  raw_value = 0U;
    fsp_err_t err       = adc_shared_read(TDS_ADC_CHANNEL, &raw_value);

    if (0U == tds_adc_read_is_valid(raw_value, err))
    {
        return TDS_ADC_INVALID_RAW;
    }

    return raw_value;
}

void tds_driver_init(void)
{
    uint16_t i;

    tds_adc_init();

    for (i = 0U; i < TDS_MEDIAN_FILTER_LEN; i++)
    {
        s_analog_buffer[i] = 0U;
    }

    s_analog_buffer_index = 0U;
    s_last_sample_tick    = xTaskGetTickCount();
    s_last_update_tick    = s_last_sample_tick;
}

void tds_driver_process(void)
{
    TickType_t now = xTaskGetTickCount();

    if ((now - s_last_sample_tick) >= pdMS_TO_TICKS(TDS_SAMPLE_INTERVAL_MS))
    {
        uint16_t raw = tds_adc_read_raw();

        s_last_sample_tick = now;

        if (TDS_ADC_INVALID_RAW == raw)
        {
            g_tds_sensor_state = 1U;
        }
        else
        {
            g_tds_adc_raw = raw;
            s_analog_buffer[s_analog_buffer_index] = raw;
            s_analog_buffer_index++;

            if (s_analog_buffer_index >= TDS_MEDIAN_FILTER_LEN)
            {
                s_analog_buffer_index = 0U;
            }

            g_tds_sensor_state = 0U;
        }
    }

    if ((now - s_last_update_tick) >= pdMS_TO_TICKS(TDS_UPDATE_INTERVAL_MS))
    {
        uint16_t median_raw;
        float    average_voltage;

        s_last_update_tick = now;

        if (0U == g_tds_sensor_state)
        {
            median_raw           = tds_median_filter(s_analog_buffer, TDS_MEDIAN_FILTER_LEN);
            g_tds_adc_median_raw = median_raw;
            average_voltage      = ((float) median_raw * TDS_ADC_VREF_V) / (float) TDS_ADC_MAX_VALUE;
            g_tds_voltage_v      = average_voltage;
            g_tds_value_ppm      = tds_voltage_to_ppm(average_voltage, g_tds_water_temp_c);
            g_tds_sample_count++;
        }
    }
}
