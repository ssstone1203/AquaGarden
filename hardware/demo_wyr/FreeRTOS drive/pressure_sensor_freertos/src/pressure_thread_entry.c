#include "pressure_thread.h"
#include "pressure_sensor.h"

#define PRESSURE_THREAD_PERIOD_MS    (20U)

/* Keil Watch variables: observe these in debug mode. */
volatile uint16_t  g_pressure_adc_raw[PRESSURE_SENSOR_COUNT] = {0U};
volatile float     g_pressure_voltage_v[PRESSURE_SENSOR_COUNT] = {0.0f};
volatile float     g_pressure_value_kg[PRESSURE_SENSOR_COUNT] = {0.0f};
volatile float     g_pressure_voltage_max_v[PRESSURE_SENSOR_COUNT] = {0.0f};
volatile uint32_t  g_pressure_sample_count = 0U;
volatile fsp_err_t g_pressure_last_err = FSP_SUCCESS;

void vApplicationStackOverflowHook(TaskHandle_t xTask, char * pcTaskName)
{
    (void) xTask;
    (void) pcTaskName;

    taskDISABLE_INTERRUPTS();
    while (1)
    {
        __NOP();
    }
}

void vApplicationDaemonTaskStartupHook(void)
{
    /* Intentionally empty: hook is required by current FreeRTOS config. */
}

/* Pressure Thread entry function */
/* pvParameters contains TaskHandle_t */
void pressure_thread_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    pressure_sample_t sample = {0};
    TickType_t last_wake_tick = xTaskGetTickCount();

    g_pressure_last_err = pressure_sensor_init();

    while (1)
    {
        if (FSP_SUCCESS == g_pressure_last_err)
        {
            g_pressure_last_err = pressure_sensor_read(&sample);
        }

        if (FSP_SUCCESS == g_pressure_last_err)
        {
            for (uint32_t i = 0; i < PRESSURE_SENSOR_COUNT; i++)
            {
                g_pressure_adc_raw[i] = sample.adc_raw[i];
                g_pressure_voltage_v[i] = sample.voltage_v[i];
                g_pressure_value_kg[i] = sample.pressure_kg[i];
                if (g_pressure_voltage_v[i] > g_pressure_voltage_max_v[i])
                {
                    g_pressure_voltage_max_v[i] = g_pressure_voltage_v[i];
                }
            }
            g_pressure_sample_count++;
        }

        vTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(PRESSURE_THREAD_PERIOD_MS));
    }
}
