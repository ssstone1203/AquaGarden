#include "water_quality_thread0.h"
#include "wqs_sensor.h"

enum
{
    WQ_MEASURE_INTERVAL_MS = 5000U
};

typedef struct
{
    wqs_cmd_t cmd;
    uint8_t initialized;
} wq_app_ctx_t;

static wq_app_ctx_t g_wq_app = {0};

static void wq_app_init_once(void)
{
    if (0U != g_wq_app.initialized)
    {
        return;
    }

    WQS_Init(&g_wq_app.cmd);
    g_wq_app.initialized = 1U;
}

static void wq_app_measure_once(void)
{
    WQS_CmdSend(&g_wq_app.cmd, WQS_CMD_TYPE_DETECT);

    g_wqs_last_read_ok = (uint8_t) (WQS_InfoGet(&g_wqs_info) ? 1U : 0U);
    g_wqs_measure_count++;
}

/* Water Quality Thread entry function */
/* pvParameters contains TaskHandle_t */
void water_quality_thread0_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    wq_app_init_once();

    while (1)
    {
        wq_app_measure_once();

        /* Datasheet requires continuous measurement interval >= 5 seconds. */
        vTaskDelay(pdMS_TO_TICKS(WQ_MEASURE_INTERVAL_MS));
    }
}
