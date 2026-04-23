#include "soil_thread.h"
#include "soil_moisture_adc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Keil Watch window variables (keep global + volatile for realtime debug). */
volatile uint16_t g_soil_adc_raw = 0U;
volatile uint8_t  g_soil_moisture_percent = 0U;
volatile uint32_t g_soil_sample_count = 0U;
volatile uint8_t  g_soil_sensor_state = 0U;      /* 0: normal, 1: adc timeout/suspect */
volatile uint16_t g_soil_last_valid_raw = 0U;
volatile uint8_t  g_soil_need_watering = 0U;     /* 1 means dry enough, should water */
volatile uint8_t  g_soil_watering_threshold = 30U;   /* Online tunable: start watering below this moisture% */
volatile uint8_t  g_soil_watering_hysteresis = 10U;  /* Online tunable: stop watering above threshold+hysteresis */

/* Internal control parameters/status (kept private to simplify Watch list). */
static uint16_t s_soil_measurable_raw_min = 0U;
static uint16_t s_soil_measurable_raw_max = 0U;
static uint8_t  s_soil_raw_in_measurable_range = 0U;

/* Module notes: wet soil -> lower ADC value. */
enum
{
    SOIL_STATE_NORMAL  = 0U,
    SOIL_STATE_TIMEOUT = 1U
};

typedef struct st_soil_sample
{
    uint16_t raw;
    uint8_t  moisture_percent;
    uint32_t sample_count;
} soil_sample_t;

static QueueHandle_t g_soil_sample_queue = NULL;

static void soil_adc_task(void * pvParameters);
static void soil_logic_task(void * pvParameters);

static uint8_t soil_detect_timeout_like_value(uint16_t raw_value)
{
    /* For 12-bit ADC, 0x0FFF is commonly seen when analog input is floating/open.
     * We use it as a simple teaching-friendly status hint for debugging. */
    if (raw_value >= 4095U)
    {
        return SOIL_STATE_TIMEOUT;
    }

    return SOIL_STATE_NORMAL;
}

static void soil_update_measurable_range(uint16_t raw_value)
{
    uint16_t dry_raw = g_soil_adc_cal_dry_raw;
    uint16_t wet_raw = g_soil_adc_cal_wet_raw;
    uint16_t min_raw = (dry_raw < wet_raw) ? dry_raw : wet_raw;
    uint16_t max_raw = (dry_raw > wet_raw) ? dry_raw : wet_raw;

    s_soil_measurable_raw_min = min_raw;
    s_soil_measurable_raw_max = max_raw;

    if ((raw_value >= min_raw) && (raw_value <= max_raw))
    {
        s_soil_raw_in_measurable_range = 1U;
    }
    else
    {
        s_soil_raw_in_measurable_range = 0U;
    }
}

void soil_thread_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    soil_adc_init();

    g_soil_sample_queue = xQueueCreate(1, sizeof(soil_sample_t));
    configASSERT(NULL != g_soil_sample_queue);

    (void) xTaskCreate(soil_adc_task,
                       "SoilADC",
                       512 / 4U,
                       NULL,
                       2,
                       NULL);

    (void) xTaskCreate(soil_logic_task,
                       "SoilLogic",
                       512 / 4U,
                       NULL,
                       1,
                       NULL);

    /* This generated thread only bootstraps user tasks, then exits. */
    vTaskDelete(NULL);
}

static void soil_adc_task(void * pvParameters)
{
    soil_sample_t sample = {0};
    TickType_t last_wake_time = 0;
    const TickType_t sample_period_ticks = pdMS_TO_TICKS(200);
    FSP_PARAMETER_NOT_USED(pvParameters);
    last_wake_time = xTaskGetTickCount();

    while (1)
    {
        g_soil_adc_raw = soil_adc_read_raw();
        g_soil_moisture_percent = soil_adc_raw_to_percent(g_soil_adc_raw);
        g_soil_sample_count++;
        soil_update_measurable_range(g_soil_adc_raw);

        g_soil_sensor_state = soil_detect_timeout_like_value(g_soil_adc_raw);
        if (SOIL_STATE_NORMAL == g_soil_sensor_state)
        {
            g_soil_last_valid_raw = g_soil_adc_raw;
        }

        sample.raw = g_soil_adc_raw;
        sample.moisture_percent = g_soil_moisture_percent;
        sample.sample_count = g_soil_sample_count;

        (void) xQueueOverwrite(g_soil_sample_queue, &sample);

        /* Fixed period sampling: more stable than relative delay in long-running systems. */
        vTaskDelayUntil(&last_wake_time, sample_period_ticks);
    }
}

static void soil_logic_task(void * pvParameters)
{
    soil_sample_t sample = {0};
    FSP_PARAMETER_NOT_USED(pvParameters);

    while (1)
    {
        if (pdPASS == xQueueReceive(g_soil_sample_queue, &sample, portMAX_DELAY))
        {
            uint8_t start_threshold = g_soil_watering_threshold;
            uint8_t hysteresis = g_soil_watering_hysteresis;
            uint16_t stop_threshold_u16 = (uint16_t) start_threshold + (uint16_t) hysteresis;
            uint8_t stop_threshold = (stop_threshold_u16 > 100U) ? 100U : (uint8_t) stop_threshold_u16;

            /* Hysteresis control:
             * - turn watering ON when moisture is below start_threshold
             * - turn watering OFF only after moisture rises above stop_threshold
             * This avoids rapid toggling near one single threshold. */
            if (0U == g_soil_need_watering)
            {
                if (sample.moisture_percent < start_threshold)
                {
                    g_soil_need_watering = 1U;
                }
            }
            else
            {
                if (sample.moisture_percent > stop_threshold)
                {
                    g_soil_need_watering = 0U;
                }
            }
        }
    }
}

/* Required by FreeRTOSConfig.h when these options are enabled:
 * - configUSE_MALLOC_FAILED_HOOK = 1
 * - configCHECK_FOR_STACK_OVERFLOW = 1 */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        /* Set a breakpoint here when malloc fails. */
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char * pcTaskName)
{
    FSP_PARAMETER_NOT_USED(xTask);
    FSP_PARAMETER_NOT_USED(pcTaskName);

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        /* Set a breakpoint here when stack overflows. */
    }
}