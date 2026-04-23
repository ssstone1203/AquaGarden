#include "pressure_sensor.h"
#include "ADC_Task.h"

/* RA6E2 ADC (12-bit), assume AVCC0 = 3.3V. */
#define PRESSURE_ADC_MAX_COUNTS          (4095.0f)
#define PRESSURE_ADC_VREF_V              (3.3f)
#define PRESSURE_SENSOR_VOLT_EPSILON     (0.02f)
#define PRESSURE_SENSOR_MIN_KG           (0.0f)
#define PRESSURE_SENSOR_OVERSAMPLE_COUNT (8U)

/*
 * Two-point calibration (must be tuned on your board):
 * - NO_LOAD: measured voltage at 0 kg.
 * - FULL_SCALE: measured voltage at 5 kg.
 *
 * This linear mapping automatically supports both slopes:
 * - FULL_SCALE > NO_LOAD  : pressure increases with voltage
 * - FULL_SCALE < NO_LOAD  : pressure decreases with voltage
 */
static const float g_pressure_voltage_no_load_v[PRESSURE_SENSOR_COUNT] =
{
    0.6f,    /* CH0 (PS0_AD) */
    0.6f,    /* CH1 (PS1_AD) */
    0.6f     /* CH2 (PS2_AD) */
};

static const float g_pressure_voltage_full_scale_v[PRESSURE_SENSOR_COUNT] =
{
    2.8f,    /* CH0 (PS0_AD) */
    2.8f,    /* CH1 (PS1_AD) */
    2.8f     /* CH2 (PS2_AD) */
};

static bool g_pressure_sensor_inited = false;

static fsp_err_t pressure_do_single_scan(void)
{
    fsp_err_t err = g_adc.p_api->scanStart(g_adc.p_ctrl);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    adc_status_t adc_status;
    do
    {
        err = g_adc.p_api->scanStatusGet(g_adc.p_ctrl, &adc_status);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    } while (ADC_STATE_SCAN_IN_PROGRESS == adc_status.state);

    return FSP_SUCCESS;
}

static float pressure_compute_kg_from_voltage(uint32_t channel_index, float voltage_v)
{
    float no_load_v = g_pressure_voltage_no_load_v[channel_index];
    float full_scale_v = g_pressure_voltage_full_scale_v[channel_index];
    float denom = full_scale_v - no_load_v;

    if ((denom < PRESSURE_SENSOR_VOLT_EPSILON) && (denom > -PRESSURE_SENSOR_VOLT_EPSILON))
    {
        return 0.0f;
    }

    float ratio = (voltage_v - no_load_v) / denom;
    if (ratio < 0.0f)
    {
        ratio = 0.0f;
    }
    else if (ratio > 1.0f)
    {
        ratio = 1.0f;
    }

    float pressure_kg = ratio * PRESSURE_SENSOR_MAX_KG;
    if (pressure_kg < PRESSURE_SENSOR_MIN_KG)
    {
        pressure_kg = PRESSURE_SENSOR_MIN_KG;
    }
    else if (pressure_kg > PRESSURE_SENSOR_MAX_KG)
    {
        pressure_kg = PRESSURE_SENSOR_MAX_KG;
    }

    return pressure_kg;
}

fsp_err_t pressure_sensor_init(void)
{
    if (g_pressure_sensor_inited)
    {
        return FSP_SUCCESS;
    }

    fsp_err_t err = g_adc.p_api->open(g_adc.p_ctrl, g_adc.p_cfg);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = g_adc.p_api->scanCfg(g_adc.p_ctrl, g_adc.p_channel_cfg);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    g_pressure_sensor_inited = true;
    return FSP_SUCCESS;
}

fsp_err_t pressure_sensor_read(pressure_sample_t * p_sample)
{
    if (NULL == p_sample)
    {
        return FSP_ERR_ASSERTION;
    }

    if (!g_pressure_sensor_inited)
    {
        fsp_err_t err_init = pressure_sensor_init();
        if (FSP_SUCCESS != err_init)
        {
            return err_init;
        }
    }

    adc_channel_t channels[PRESSURE_SENSOR_COUNT] = {
        ADC_CHANNEL_0,
        ADC_CHANNEL_1,
        ADC_CHANNEL_2
    };

    uint32_t raw_accum[PRESSURE_SENSOR_COUNT] = {0U};

    /*
     * Dummy scan for mux/sample capacitor settling.
     * This helps reduce inter-channel coupling on higher-impedance analog sources.
     */
    fsp_err_t err = pressure_do_single_scan();
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    for (uint32_t sample_idx = 0; sample_idx < PRESSURE_SENSOR_OVERSAMPLE_COUNT; sample_idx++)
    {
        err = pressure_do_single_scan();
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        for (uint32_t i = 0; i < PRESSURE_SENSOR_COUNT; i++)
        {
            uint16_t raw = 0U;
            err = g_adc.p_api->read(g_adc.p_ctrl, channels[i], &raw);
            if (FSP_SUCCESS != err)
            {
                return err;
            }

            raw_accum[i] += raw;
        }
    }

    for (uint32_t i = 0; i < PRESSURE_SENSOR_COUNT; i++)
    {
        uint16_t raw = (uint16_t) (raw_accum[i] / PRESSURE_SENSOR_OVERSAMPLE_COUNT);
        float voltage_v = ((float) raw * PRESSURE_ADC_VREF_V) / PRESSURE_ADC_MAX_COUNTS;
        if (voltage_v < 0.0f)
        {
            voltage_v = 0.0f;
        }
        else if (voltage_v > PRESSURE_ADC_VREF_V)
        {
            voltage_v = PRESSURE_ADC_VREF_V;
        }

        p_sample->adc_raw[i] = raw;
        p_sample->voltage_v[i] = voltage_v;
        p_sample->pressure_kg[i] = pressure_compute_kg_from_voltage(i, p_sample->voltage_v[i]);
    }

    return FSP_SUCCESS;
}
