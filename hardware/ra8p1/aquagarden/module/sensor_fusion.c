#include "sensor_fusion.h"
#include <stddef.h>

void sensor_fusion_eval(const fusion_config_t * p_cfg,
                        const fusion_inputs_t * p_in,
                        fusion_outputs_t * p_out)
{
    uint8_t need;

    if ((NULL == p_cfg) || (NULL == p_in) || (NULL == p_out))
    {
        return;
    }

    /* Start from the previous decision so soil control keeps its hysteresis latch. */
    need = (0U != p_in->prev_need_watering) ? 1U : 0U;

    if ((0U != p_cfg->enable_soil) && p_in->soil_valid)
    {
        if (p_in->soil_pct <= p_cfg->soil_threshold)
        {
            need = 1U;
        }
        else if (p_in->soil_pct >= (uint8_t) (p_cfg->soil_threshold + p_cfg->soil_hysteresis))
        {
            need = 0U;
        }
    }

    p_out->water_temp_high = (0U != p_cfg->enable_water_temp) &&
                             p_in->water_temp_valid &&
                             (p_in->water_temp_c >= p_cfg->water_temp_high_c);

    p_out->tds_low = (0U != p_cfg->enable_tds) &&
                     p_in->tds_valid &&
                     (p_in->tds_ntu <= p_cfg->tds_low_ntu);

    if (p_out->water_temp_high || p_out->tds_low)
    {
        need = 1U;
    }

    p_out->need_watering = need;
    p_out->auto_pump_pwm = (0U != need) ? p_cfg->auto_pump_pwm : 0U;
}
