#include "Pump_Task.h"
#include "pump_drv8870.h"
#include "sensor_fusion.h"
#include "wqs_sensor.h"
#include "ds18b20.h"
                /* Pump_Driver entry function */
                /* pvParameters contains TaskHandle_t */
                void Pump_Task_entry(void * pvParameters)
                {
                    FSP_PARAMETER_NOT_USED(pvParameters);
                    pump_drv8870_init();

                    uint8_t last_power = 0xFFU;
                    const TickType_t control_period_ticks = pdMS_TO_TICKS(200U);
                    while(1)
                    {
                        uint8_t need_watering = 0U;

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

                        if (0U != g_pump_manual_mode)
                        {
                            g_pump_target_power_percent = g_pump_manual_power_percent;
                        }
                        else
                        {
                            g_pump_target_power_percent = (0U != g_control_need_watering) ? 70U : 0U;
                        }

                        if (g_pump_target_power_percent != last_power)
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
