#include "Pump_Task.h"
#include "pump_drv8870.h"
#include "sensor_fusion.h"
#include "wqs_sensor.h"
#include "ds18b20.h"

#define PUMP_CYCLE_STATE_IDLE      (0U)
#define PUMP_CYCLE_STATE_RUN       (1U)
#define PUMP_CYCLE_STATE_STOP      (2U)
#define PUMP_CYCLE_STATE_INTERVAL  (3U)
#define PUMP_CYCLE_STATE_DONE      (4U)

/* Pump_Driver entry function */
/* pvParameters contains TaskHandle_t */
void Pump_Task_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);
    pump_drv8870_init();

    uint8_t last_power = 0xFFU;
    const TickType_t control_period_ticks = pdMS_TO_TICKS(200U);
    TickType_t cycle_state_enter_tick = 0U;
    while (1)
    {
        uint8_t need_watering = 0U;
        uint8_t timed_mode_in_control = 0U;
        TickType_t now_tick = xTaskGetTickCount();

        if ((0U != g_ctrl_enable_soil) && (0U != g_soil_need_watering))
        {
            need_watering = 1U;
        }

        if ((0U != g_ctrl_enable_water_temp) &&
            (FSP_SUCCESS == g_uwt_last_err) &&
            (g_uwt_temperature_c >= g_ctrl_water_temp_high_c))
        {
            need_watering = 1U;
        }

        if ((0U != g_ctrl_enable_wqi) &&
            (0U != g_wqs_last_read_ok) &&
            (g_wqs_info.wqs_info_wqi <= g_ctrl_wqi_low_threshold))
        {
            need_watering = 1U;
        }

        g_control_need_watering = need_watering;

        if (0U == g_pump_cycle_enable)
        {
            g_pump_cycle_active = 0U;
            g_pump_cycle_state = PUMP_CYCLE_STATE_IDLE;
            g_pump_cycle_done_count = 0U;
        }
        else
        {
            if ((0U == g_pump_cycle_start) && (0U != g_pump_cycle_active))
            {
                g_pump_cycle_active = 0U;
                g_pump_cycle_state = PUMP_CYCLE_STATE_IDLE;
                g_pump_cycle_done_count = 0U;
            }

            if ((0U != g_pump_cycle_start) && (0U == g_pump_cycle_active))
            {
                g_pump_cycle_active = 1U;
                g_pump_cycle_state = PUMP_CYCLE_STATE_RUN;
                g_pump_cycle_done_count = 0U;
                cycle_state_enter_tick = now_tick;
            }
        }

        if (0U != g_pump_cycle_active)
        {
            uint32_t elapsed_ms = (uint32_t) ((now_tick - cycle_state_enter_tick) * portTICK_PERIOD_MS);
            uint32_t run_ms = g_pump_cycle_run_time_ms;
            uint32_t stop_ms = g_pump_cycle_stop_time_ms;
            uint32_t interval_ms = g_pump_cycle_interval_time_ms;

            timed_mode_in_control = 1U;
            switch (g_pump_cycle_state)
            {
                case PUMP_CYCLE_STATE_RUN:
                    g_pump_target_power_percent = g_pump_cycle_power_percent;
                    if (g_pump_target_power_percent > 100U)
                    {
                        g_pump_target_power_percent = 100U;
                    }

                    if (elapsed_ms >= run_ms)
                    {
                        g_pump_cycle_state = PUMP_CYCLE_STATE_STOP;
                        cycle_state_enter_tick = now_tick;
                    }
                    break;

                case PUMP_CYCLE_STATE_STOP:
                    g_pump_target_power_percent = 0U;
                    if (elapsed_ms >= stop_ms)
                    {
                        g_pump_cycle_done_count++;
                        if ((g_pump_cycle_total_count > 0U) &&
                            (g_pump_cycle_done_count >= g_pump_cycle_total_count))
                        {
                            g_pump_cycle_active = 0U;
                            g_pump_cycle_state = PUMP_CYCLE_STATE_DONE;
                            g_pump_cycle_start = 0U;
                        }
                        else if (interval_ms > 0U)
                        {
                            g_pump_cycle_state = PUMP_CYCLE_STATE_INTERVAL;
                            cycle_state_enter_tick = now_tick;
                        }
                        else
                        {
                            g_pump_cycle_state = PUMP_CYCLE_STATE_RUN;
                            cycle_state_enter_tick = now_tick;
                        }
                    }
                    break;

                case PUMP_CYCLE_STATE_INTERVAL:
                    g_pump_target_power_percent = 0U;
                    if (elapsed_ms >= interval_ms)
                    {
                        g_pump_cycle_state = PUMP_CYCLE_STATE_RUN;
                        cycle_state_enter_tick = now_tick;
                    }
                    break;

                default:
                    g_pump_target_power_percent = 0U;
                    g_pump_cycle_active = 0U;
                    g_pump_cycle_state = PUMP_CYCLE_STATE_IDLE;
                    g_pump_cycle_done_count = 0U;
                    break;
            }
        }

        if (0U == timed_mode_in_control)
        {
            if (0U != g_pump_manual_mode)
            {
                g_pump_target_power_percent = g_pump_manual_power_percent;
            }
            else
            {
                g_pump_target_power_percent = (0U != g_control_need_watering) ? 70U : 0U;
            }
        }

        if ((g_pump_target_power_percent != last_power) || (0U == g_pump_target_power_percent))
        {
            pump_drv8870_set_power(g_pump_target_power_percent);
            last_power = g_pump_target_power_percent;
        }

        g_pump_actual_power_percent = g_pump_target_power_percent;
        g_pump_running              = (g_pump_actual_power_percent > 0U) ? 1U : 0U;
        g_jscope_pump_power_pct     = (float) g_pump_actual_power_percent;

        sensor_fusion_update_jscope_time();
        vTaskDelay(control_period_ticks);
    }
}
