#include "new_thread0.h"

#include "hal_data.h"
#include "sht30.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef APP_SHT30_SAMPLE_PERIOD_MS
#define APP_SHT30_SAMPLE_PERIOD_MS (500U)
#endif

static void app_sensor_init(void)
{
    fsp_err_t err = sht30_init(&g_i2c_master0, SHT30_I2C_ADDR_7BIT_DEFAULT);
    if (FSP_SUCCESS != err)
    {
        (void) err;
    }
}

static void app_sensor_poll_once(void)
{
    float     t_c = 0.0F;
    float     rh  = 0.0F;
    fsp_err_t err = sht30_measure_single_shot(&t_c, &rh, NULL, true);

    (void) err;
}

void new_thread0_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    app_sensor_init();

    for (;;)
    {
        app_sensor_poll_once();
        vTaskDelay(pdMS_TO_TICKS(APP_SHT30_SAMPLE_PERIOD_MS));
    }
}
