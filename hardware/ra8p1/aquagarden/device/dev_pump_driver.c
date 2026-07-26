#include "dev_pump_driver.h"
#include "../ra_gen/common_data.h"
#include "../ra_gen/main_service.h"
#include "r_gpt.h"
#include <stdbool.h>

#define PUMP_IN1_PIN BSP_IO_PORT_01_PIN_04
#define PUMP_IN2_PIN BSP_IO_PORT_01_PIN_05

static bool s_pump_opened;
static uint8_t s_pump_pwm;

static void pump_pin_gpio_high(bsp_io_port_pin_t pin)
{
    (void) R_IOPORT_PinCfg(&g_ioport_ctrl, pin,
                           IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH);
    (void) R_IOPORT_PinWrite(&g_ioport_ctrl, pin, BSP_IO_LEVEL_HIGH);
}

static void pump_in2_as_gpt(void)
{
    (void) R_IOPORT_PinCfg(&g_ioport_ctrl, PUMP_IN2_PIN,
                           IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_GPT1);
}

fsp_err_t dev_pump_brake(void)
{
    /* DRV8870 brake = IN1=1 and IN2=1.
     * Use pure GPIO highs. Do NOT mux IN2 to GPT here: GPT stop-level / default
     * duty can leave IN2 low and create forward (IN1=1, IN2=0). */
    if (s_pump_opened)
    {
        (void) g_pump_timer.p_api->stop(g_pump_timer.p_ctrl);
    }

    /* IN2 first, then IN1, so we never pass through forward. */
    pump_pin_gpio_high(PUMP_IN2_PIN);
    pump_pin_gpio_high(PUMP_IN1_PIN);

    s_pump_pwm = 0U;
    return FSP_SUCCESS;
}

void dev_pump_init(void)
{
    (void) dev_pump_brake();

    if (!s_pump_opened)
    {
        if (FSP_SUCCESS == g_pump_timer.p_api->open(g_pump_timer.p_ctrl, g_pump_timer.p_cfg))
        {
            s_pump_opened = true;
        }
    }

    /* Keep pins as GPIO brake even after GPT open (do not enable PWM pin mux). */
    (void) dev_pump_brake();
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
        return dev_pump_brake();
    }

    /* Prepare GPT output first while IN2 is still GPIO-high (brake). */
    duty_counts = ((uint32_t) g_pump_timer_cfg.period_counts * (uint32_t) pwm_percent) / 100UL;
    err = g_pump_timer.p_api->dutyCycleSet(g_pump_timer.p_ctrl, duty_counts, GPT_IO_PIN_GTIOCA);
    if (FSP_SUCCESS != err)
    {
        (void) dev_pump_brake();
        return err;
    }

    err = g_pump_timer.p_api->start(g_pump_timer.p_ctrl);
    if (FSP_SUCCESS != err)
    {
        (void) dev_pump_brake();
        return err;
    }

    pump_pin_gpio_high(PUMP_IN1_PIN);
    pump_in2_as_gpt();

    s_pump_pwm = pwm_percent;
    return FSP_SUCCESS;
}

uint8_t dev_pump_get_pwm(void)
{
    return s_pump_pwm;
}
