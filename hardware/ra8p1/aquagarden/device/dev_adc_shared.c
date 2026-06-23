#include "dev_adc_shared.h"
#include "../ra_gen/main_service.h"
#include <stdbool.h>

static bool s_adc_opened;
static bool s_adc_scanning;

void dev_adc_shared_init(void)
{
    if (!s_adc_opened)
    {
        if (FSP_SUCCESS == g_adc0.p_api->open(g_adc0.p_ctrl, g_adc0.p_cfg))
        {
            s_adc_opened = true;
        }
    }

    if (s_adc_opened && !s_adc_scanning)
    {
        if (FSP_SUCCESS == g_adc0.p_api->scanCfg(g_adc0.p_ctrl, &g_adc0_scan_cfg))
        {
            (void) g_adc0.p_api->scanStart(g_adc0.p_ctrl);
            s_adc_scanning = true;
        }
    }
}
