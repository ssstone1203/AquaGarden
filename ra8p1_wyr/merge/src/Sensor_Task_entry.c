#include "Sensor_Task.h"
#include "soil_moisture_adc.h"
#include "sht30.h"
#include "ds18b20.h"
#include "dev_tds_driver.h"
#include "adc_shared.h"
#include "sensor_fusion.h"
#include "app_startup.h"
#include "hal_data.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef SOIL_SAMPLE_PERIOD_MS
#define SOIL_SAMPLE_PERIOD_MS (200U)
#endif
#ifndef APP_SHT30_SAMPLE_PERIOD_MS
#define APP_SHT30_SAMPLE_PERIOD_MS (500U)
#endif
#ifndef UWT_CONVERT_WAIT_MS
#define UWT_CONVERT_WAIT_MS (800U)
#endif
#ifndef UWT_SAMPLE_PERIOD_MS
#define UWT_SAMPLE_PERIOD_MS (2000U)
#endif

enum
{
    SOIL_STATE_NORMAL  = 0U,
    SOIL_STATE_TIMEOUT = 1U,
    UWT_STATE_IDLE     = 0U,
    UWT_STATE_WAIT_CONVERT = 1U,
};

static uint8_t soil_detect_timeout_like_value(uint16_t raw_value)
{
    if (raw_value >= SOIL_ADC_MAX_VALUE)
    {
        return SOIL_STATE_TIMEOUT;
    }

    return SOIL_STATE_NORMAL;
}

static void soil_update_watering_logic(uint8_t moisture_percent)
{
    uint8_t  start_threshold    = g_soil_watering_threshold;
    uint8_t  hysteresis           = g_soil_watering_hysteresis;
    uint16_t stop_threshold_u16   = (uint16_t) start_threshold + (uint16_t) hysteresis;
    uint8_t  stop_threshold       = (stop_threshold_u16 > 100U) ? 100U : (uint8_t) stop_threshold_u16;

    if (0U == g_soil_need_watering)
    {
        if (moisture_percent < start_threshold)
        {
            g_soil_need_watering = 1U;
        }
    }
    else
    {
        if (moisture_percent > stop_threshold)
        {
            g_soil_need_watering = 0U;
        }
    }
}

static void sensor_poll_soil(TickType_t now, TickType_t * p_last_tick)
{
    if ((now - *p_last_tick) < pdMS_TO_TICKS(SOIL_SAMPLE_PERIOD_MS))
    {
        return;
    }

    *p_last_tick = now;

    g_soil_adc_raw            = soil_adc_read_raw();
    g_soil_moisture_percent   = soil_adc_raw_to_percent(g_soil_adc_raw);
    g_soil_sample_count++;
    g_jscope_soil_percent     = g_soil_moisture_percent;
    g_soil_sensor_state       = soil_detect_timeout_like_value(g_soil_adc_raw);

    if (SOIL_STATE_NORMAL == g_soil_sensor_state)
    {
        g_soil_last_valid_raw = g_soil_adc_raw;
    }

    soil_update_watering_logic(g_soil_moisture_percent);
}

static void sensor_poll_ths(TickType_t now, TickType_t * p_last_tick, bool * p_ready)
{
    fsp_err_t err;

    if ((now - *p_last_tick) < pdMS_TO_TICKS(APP_SHT30_SAMPLE_PERIOD_MS))
    {
        return;
    }

    *p_last_tick = now;

    if (!(*p_ready))
    {
        err = sht30_init(&g_i2c_master0, SHT30_I2C_ADDR_7BIT_DEFAULT);
        if ((FSP_SUCCESS == err) || (FSP_ERR_ALREADY_OPEN == err))
        {
            *p_ready = true;
        }
        else
        {
            g_air_last_err = err;
            return;
        }
    }

    float    temp_c = 0.0F;
    float    rh     = 0.0F;
    uint32_t retry  = 0U;

    do
    {
        err = sht30_measure_single_shot(&temp_c, &rh, NULL, true);
        if (FSP_SUCCESS == err)
        {
            break;
        }

        retry++;
        vTaskDelay(pdMS_TO_TICKS(20U));
    } while (retry < 2U);

    g_air_retry_count = retry;
    g_air_last_err    = err;

    if (FSP_SUCCESS == err)
    {
        g_jscope_air_temp_c      = temp_c;
        g_jscope_air_humidity_rh = rh;
    }
    else if ((FSP_ERR_NOT_INITIALIZED == err) || (FSP_ERR_NOT_OPEN == err))
    {
        *p_ready = false;
    }
}

static void sensor_poll_uwt(TickType_t now, TickType_t * p_last_cycle, uint8_t * p_state, TickType_t * p_convert_start)
{
    switch (*p_state)
    {
    case UWT_STATE_IDLE:
        if ((now - *p_last_cycle) >= pdMS_TO_TICKS(UWT_SAMPLE_PERIOD_MS))
        {
            if (FSP_SUCCESS == DS18B20_ConvertT_Start())
            {
                *p_state           = UWT_STATE_WAIT_CONVERT;
                *p_convert_start   = now;
            }
            else
            {
                g_uwt_retry_count++;
                *p_last_cycle = now;
            }
        }
        break;

    case UWT_STATE_WAIT_CONVERT:
        if ((now - *p_convert_start) >= pdMS_TO_TICKS(UWT_CONVERT_WAIT_MS))
        {
            g_uwt_last_err = DS18B20_ReadResult();
            if (FSP_SUCCESS == g_uwt_last_err)
            {
                g_jscope_water_temp_c = g_uwt_temperature_c;
            }

            *p_state      = UWT_STATE_IDLE;
            *p_last_cycle = now;
        }
        break;

    default:
        *p_state = UWT_STATE_IDLE;
        break;
    }
}

static void sensor_poll_tds(void)
{
    if (FSP_SUCCESS == g_uwt_last_err)
    {
        g_tds_water_temp_c = g_uwt_temperature_c;
    }

    tds_driver_process();

    if (0U == g_tds_sensor_state)
    {
        g_tds_last_err   = FSP_SUCCESS;
        g_jscope_tds_ppm = g_tds_value_ppm;
        g_tds_wqi        = sensor_fusion_tds_ppm_to_wqi(g_tds_value_ppm);
        g_jscope_tds_wqi = g_tds_wqi;
    }
    else
    {
        g_tds_last_err = (0U == g_adc_shared_ready) ? FSP_ERR_NOT_INITIALIZED : FSP_ERR_TIMEOUT;
        g_tds_retry_count++;
    }
}

void Sensor_Task_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    bool       ths_ready     = false;
    uint8_t    uwt_state     = UWT_STATE_IDLE;
    TickType_t last_soil     = xTaskGetTickCount();
    TickType_t last_ths      = last_soil;
    TickType_t last_uwt      = last_soil;
    TickType_t uwt_convert   = last_soil;

    adc_shared_init();
    soil_adc_init();
    tds_driver_init();
    DS18B20_Init();

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();

        g_task_hb_sensor++;

        sensor_poll_soil(now, &last_soil);
        sensor_poll_ths(now, &last_ths, &ths_ready);
        sensor_poll_uwt(now, &last_uwt, &uwt_state, &uwt_convert);
        sensor_poll_tds();

        sensor_fusion_update_jscope_time();
        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}
