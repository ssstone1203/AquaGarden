#include "new_thread0.h"
#include "soil_moisture_adc.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef SOIL_SAMPLE_PERIOD_MS
#define SOIL_SAMPLE_PERIOD_MS (200U)
#endif

/* Keil Watch window variables (keep global + volatile for realtime debug). */
volatile uint16_t g_soil_adc_raw = 0U;
volatile uint8_t  g_soil_moisture_percent = 0U;
volatile uint32_t g_soil_sample_count = 0U;
volatile uint8_t  g_soil_sensor_state = 0U;      /* 0: normal, 1: adc timeout/suspect */
volatile uint16_t g_soil_last_valid_raw = 0U;
volatile uint8_t  g_soil_need_watering = 0U;     /* 1 means dry enough, should water */
volatile uint8_t  g_soil_watering_threshold = 30U;
volatile uint8_t  g_soil_watering_hysteresis = 10U;

enum
{
    SOIL_STATE_NORMAL  = 0U,
    SOIL_STATE_TIMEOUT = 1U
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
    uint8_t  start_threshold = g_soil_watering_threshold;
    uint8_t  hysteresis = g_soil_watering_hysteresis;
    uint16_t stop_threshold_u16 = (uint16_t) start_threshold + (uint16_t) hysteresis;
    uint8_t  stop_threshold = (stop_threshold_u16 > 100U) ? 100U : (uint8_t) stop_threshold_u16;

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

void new_thread0_entry(void * pvParameters)
{
    TickType_t last_wake_time;
    FSP_PARAMETER_NOT_USED(pvParameters);

    soil_adc_init();
    last_wake_time = xTaskGetTickCount();

    for (;;)
    {
        g_soil_adc_raw = soil_adc_read_raw();
        g_soil_moisture_percent = soil_adc_raw_to_percent(g_soil_adc_raw);
        g_soil_sample_count++;

        g_soil_sensor_state = soil_detect_timeout_like_value(g_soil_adc_raw);
        if (SOIL_STATE_NORMAL == g_soil_sensor_state)
        {
            g_soil_last_valid_raw = g_soil_adc_raw;
        }

        soil_update_watering_logic(g_soil_moisture_percent);

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SOIL_SAMPLE_PERIOD_MS));
    }
}
