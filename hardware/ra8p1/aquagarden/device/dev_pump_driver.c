#include "dev_pump_driver.h"
#include "../ra_gen/common_data.h"
#include "../ra_gen/main_service.h"
#include "r_gpt.h"
#include <stdbool.h>

#define PUMP_IN1_PIN BSP_IO_PORT_01_PIN_04

static bool s_pump_opened;
static uint8_t s_pump_pwm;

void dev_pump_init(void)
{
    if (!s_pump_opened)
    {
        (void) R_IOPORT_PinCfg(&g_ioport_ctrl, PUMP_IN1_PIN,
                               IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);
        if (FSP_SUCCESS == g_pump_timer.p_api->open(g_pump_timer.p_ctrl, g_pump_timer.p_cfg))
        {
            s_pump_opened = true;
        }
    }

    if (s_pump_opened)
    {
        (void) dev_pump_set_pwm(0U);
    }
}

fsp_err_t dev_pump_set_pwm(uint8_t pwm_percent)
{
    uint32_t duty_counts;
    fsp_err_t err;

    if (pwm_percent > 100U)
    {
        pwm_percent = 100U;
    }

    if (!s_pump_opened)
    {
        dev_pump_init();
        if (!s_pump_opened)
        {
            return FSP_ERR_NOT_OPEN;
        }
    }

    if (0U == pwm_percent)
    {
        (void) g_pump_timer.p_api->stop(g_pump_timer.p_ctrl);
        (void) R_IOPORT_PinWrite(&g_ioport_ctrl, PUMP_IN1_PIN, BSP_IO_LEVEL_LOW);
        s_pump_pwm = 0U;
        return FSP_SUCCESS;
    }

    (void) R_IOPORT_PinWrite(&g_ioport_ctrl, PUMP_IN1_PIN, BSP_IO_LEVEL_HIGH);
    duty_counts = ((uint32_t) g_pump_timer_cfg.period_counts * (uint32_t) pwm_percent) / 100UL;
    err = g_pump_timer.p_api->dutyCycleSet(g_pump_timer.p_ctrl, duty_counts, GPT_IO_PIN_GTIOCA);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = g_pump_timer.p_api->start(g_pump_timer.p_ctrl);
    if (FSP_SUCCESS == err)
    {
        s_pump_pwm = pwm_percent;
    }

    return err;
}

uint8_t dev_pump_get_pwm(void)
{
    return s_pump_pwm;
}
