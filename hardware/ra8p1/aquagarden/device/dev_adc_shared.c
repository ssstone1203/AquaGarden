#include "dev_adc_shared.h"
#include "../ra_gen/main_service.h"
#include "bsp_api.h"
#include <stdbool.h>

/* RA8P1 ADC_B requires calibrate before scanStart (hardware/demo/soil_sensor uses
 * classic R_ADC on RA6 and only needs Open+ScanCfg+ScanStart). */
#define ADC_SHARED_CAL_TIMEOUT_MS (1000U)

static bool s_adc_opened;
static bool s_adc_ready;

static fsp_err_t adc_shared_wait_idle(void)
{
    adc_status_t status;
    uint32_t     timeout_ms = ADC_SHARED_CAL_TIMEOUT_MS;

    while (timeout_ms > 0U)
    {
        fsp_err_t err = g_adc0.p_api->scanStatusGet(g_adc0.p_ctrl, &status);
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

void dev_adc_shared_init(void)
{
    fsp_err_t err;

    if (s_adc_ready)
    {
        return;
    }

    /* demo SHS_Init: Open + ScanCfg */
    if (!s_adc_opened)
    {
        err = g_adc0.p_api->open(g_adc0.p_ctrl, g_adc0.p_cfg);
        if (FSP_SUCCESS != err)
        {
            return;
        }
        s_adc_opened = true;
    }

    err = g_adc0.p_api->scanCfg(g_adc0.p_ctrl, &g_adc0_scan_cfg);
    if (FSP_SUCCESS != err)
    {
        return;
    }

    /* RA8P1 ADC_B only: self-calibration before first scan. */
    err = g_adc0.p_api->calibrate(g_adc0.p_ctrl, NULL);
    if (FSP_SUCCESS != err)
    {
        return;
    }

    if (FSP_SUCCESS != adc_shared_wait_idle())
    {
        return;
    }

    /* demo SHS_ScanStart: continuous scan (configured in RASC). */
    err = g_adc0.p_api->scanStart(g_adc0.p_ctrl);
    if (FSP_SUCCESS != err)
    {
        return;
    }

    s_adc_ready = true;
}

bool dev_adc_shared_is_ready(void)
{
    return s_adc_ready;
}
