#include "adc_shared.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "r_adc_b.h"

extern adc_b_instance_ctrl_t      g_adc_b_ctrl;
extern const adc_cfg_t            g_adc_b_cfg;
extern const adc_b_scan_cfg_t     g_adc_b_scan_cfg;
extern const adc_api_t            g_adc_on_adc_b;

/* Keep FSP API vtable linked even when g_adc_b instance struct is GC'd. */
static void const * const s_keep_adc_api __attribute__((used)) = &g_adc_on_adc_b;

enum
{
    ADC_SHARED_CALIBRATION_TIMEOUT_MS = 1000U,
    ADC_SHARED_RAW_MASK               = 0x0FFFU
};

static SemaphoreHandle_t s_adc_mutex;
static StaticSemaphore_t s_adc_mutex_buf;
static uint8_t           s_adc_hw_ready;

volatile uint8_t   g_adc_shared_ready     = 0U;
volatile uint8_t   g_adc_shared_init_step = 0U;
volatile fsp_err_t g_adc_shared_last_err  = FSP_SUCCESS;

static uint16_t adc_shared_normalize_raw(uint16_t raw)
{
    return (uint16_t) (raw & ADC_SHARED_RAW_MASK);
}

static fsp_err_t adc_shared_wait_for_idle(void)
{
    adc_status_t status     = {0};
    uint32_t     timeout_ms = ADC_SHARED_CALIBRATION_TIMEOUT_MS;

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

static void adc_shared_init_mutex(void)
{
    if ((NULL != s_adc_mutex) || (taskSCHEDULER_NOT_STARTED == xTaskGetSchedulerState()))
    {
        return;
    }

    s_adc_mutex = xSemaphoreCreateMutexStatic(&s_adc_mutex_buf);
}

void adc_shared_init(void)
{
    fsp_err_t err;

    if (0U != s_adc_hw_ready)
    {
        g_adc_shared_ready     = 1U;
        g_adc_shared_init_step = 6U;
        adc_shared_init_mutex();
        return;
    }

    g_adc_shared_init_step = 1U;

    g_adc_shared_init_step = 2U;
    err = R_ADC_B_Open(&g_adc_b_ctrl, &g_adc_b_cfg);
    if (FSP_SUCCESS != err)
    {
        g_adc_shared_last_err = err;
        return;
    }

    g_adc_shared_init_step = 3U;
    err = R_ADC_B_ScanCfg(&g_adc_b_ctrl, &g_adc_b_scan_cfg);
    if (FSP_SUCCESS != err)
    {
        g_adc_shared_last_err = err;
        return;
    }

    g_adc_shared_init_step = 4U;
    err = R_ADC_B_Calibrate(&g_adc_b_ctrl, NULL);
    if (FSP_SUCCESS != err)
    {
        g_adc_shared_last_err = err;
        return;
    }

    g_adc_shared_init_step = 5U;
    err = adc_shared_wait_for_idle();
    if (FSP_SUCCESS != err)
    {
        g_adc_shared_last_err = err;
        return;
    }

    s_adc_hw_ready             = 1U;
    g_adc_shared_ready         = 1U;
    g_adc_shared_init_step     = 6U;
    g_adc_shared_last_err      = FSP_SUCCESS;
    adc_shared_init_mutex();
}

fsp_err_t adc_shared_read(uint8_t channel, uint16_t * raw_out)
{
    fsp_err_t err;

    if ((NULL == raw_out) || (0U == s_adc_hw_ready))
    {
        g_adc_shared_last_err = FSP_ERR_NOT_INITIALIZED;
        return FSP_ERR_NOT_INITIALIZED;
    }

    adc_shared_init_mutex();
    if (NULL == s_adc_mutex)
    {
        g_adc_shared_last_err = FSP_ERR_NOT_INITIALIZED;
        return FSP_ERR_NOT_INITIALIZED;
    }

    if (pdTRUE != xSemaphoreTake(s_adc_mutex, pdMS_TO_TICKS(500U)))
    {
        g_adc_shared_last_err = FSP_ERR_TIMEOUT;
        return FSP_ERR_TIMEOUT;
    }

    err = R_ADC_B_ScanStart(&g_adc_b_ctrl);
    if (FSP_SUCCESS == err)
    {
        err = adc_shared_wait_for_idle();
    }

    if (FSP_SUCCESS == err)
    {
        err = R_ADC_B_Read(&g_adc_b_ctrl, channel, raw_out);
        if ((FSP_SUCCESS == err) || (FSP_ERR_INVALID_DATA == err))
        {
            *raw_out = adc_shared_normalize_raw(*raw_out);
            g_adc_shared_last_err = (FSP_ERR_INVALID_DATA == err) ? err : FSP_SUCCESS;
        }
        else
        {
            g_adc_shared_last_err = err;
        }
    }
    else
    {
        g_adc_shared_last_err = err;
    }

    (void) xSemaphoreGive(s_adc_mutex);
    return err;
}
