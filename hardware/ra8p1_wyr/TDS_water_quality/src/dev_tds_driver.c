#include "dev_tds_driver.h"

#include "FreeRTOS.h"
#include "task.h"

enum
{
    TDS_ADC_CALIBRATION_TIMEOUT_MS = 1000U
};

/* Keil Watch window variables (global + volatile for realtime debug). */
volatile uint16_t g_tds_adc_raw           = 0U;
volatile uint16_t g_tds_adc_median_raw    = 0U;
volatile float    g_tds_voltage_v         = 0.0f;
volatile float    g_tds_value_ppm         = 0.0f;
volatile uint32_t g_tds_sample_count      = 0U;
volatile uint8_t  g_tds_sensor_state      = 0U; /* 0: normal, 1: adc timeout/suspect */
volatile float    g_tds_water_temp_c      = 25.0f;

static uint16_t   s_analog_buffer[TDS_MEDIAN_FILTER_LEN];
static uint16_t   s_analog_buffer_index   = 0U;
static TickType_t s_last_sample_tick      = 0U;
static TickType_t s_last_update_tick      = 0U;

static fsp_err_t tds_adc_wait_for_idle(void)
{
    adc_status_t status     = {0};
    uint32_t     timeout_ms = TDS_ADC_CALIBRATION_TIMEOUT_MS;

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

static uint16_t tds_median_filter(uint16_t const * samples, uint16_t length)
{
    uint16_t sorted[TDS_MEDIAN_FILTER_LEN];
    uint16_t i;
    uint16_t j;

    for (i = 0U; i < length; i++)
    {
        sorted[i] = samples[i];
    }

    for (j = 0U; j < (length - 1U); j++)
    {
        for (i = 0U; i < ((length - j) - 1U); i++)
        {
            if (sorted[i] > sorted[i + 1U])
            {
                uint16_t temp   = sorted[i];
                sorted[i]       = sorted[i + 1U];
                sorted[i + 1U] = temp;
            }
        }
    }

    if ((length & 1U) > 0U)
    {
        return sorted[(length - 1U) / 2U];
    }

    return (uint16_t) (((uint32_t) sorted[length / 2U] + (uint32_t) sorted[(length / 2U) - 1U]) / 2U);
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

void tds_adc_init(void)
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

    (void) tds_adc_wait_for_idle();
}

uint16_t tds_adc_read_raw(void)
{
    fsp_err_t err;
    uint16_t  raw_value = TDS_ADC_MAX_VALUE;

    err = R_ADC_B_ScanStart(&g_adc_b_ctrl);
    if (FSP_SUCCESS != err)
    {
        return TDS_ADC_MAX_VALUE;
    }

    err = tds_adc_wait_for_idle();
    if (FSP_SUCCESS != err)
    {
        return TDS_ADC_MAX_VALUE;
    }

    err = R_ADC_B_Read(&g_adc_b_ctrl, TDS_ADC_CHANNEL, &raw_value);
    if (FSP_SUCCESS != err)
    {
        return TDS_ADC_MAX_VALUE;
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
        s_last_sample_tick = now;

        g_tds_adc_raw = tds_adc_read_raw();
        s_analog_buffer[s_analog_buffer_index] = g_tds_adc_raw;
        s_analog_buffer_index++;

        if (s_analog_buffer_index >= TDS_MEDIAN_FILTER_LEN)
        {
            s_analog_buffer_index = 0U;
        }

        if (g_tds_adc_raw >= TDS_ADC_MAX_VALUE)
        {
            g_tds_sensor_state = 1U;
        }
        else
        {
            g_tds_sensor_state = 0U;
        }
    }

    if ((now - s_last_update_tick) >= pdMS_TO_TICKS(TDS_UPDATE_INTERVAL_MS))
    {
        uint16_t median_raw;
        float    average_voltage;

        s_last_update_tick = now;

        median_raw       = tds_median_filter(s_analog_buffer, TDS_MEDIAN_FILTER_LEN);
        g_tds_adc_median_raw = median_raw;
        average_voltage  = ((float) median_raw * TDS_ADC_VREF_V) / (float) TDS_ADC_MAX_VALUE;
        g_tds_voltage_v  = average_voltage;
        g_tds_value_ppm  = tds_voltage_to_ppm(average_voltage, g_tds_water_temp_c);
        g_tds_sample_count++;
    }
}
