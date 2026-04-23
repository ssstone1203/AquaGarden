#include "ADC_Task.h"
#include "pressure_sensor.h"
#include "soil_moisture_adc.h"
#include "ds18b20.h"
#include "sensor_fusion.h"

void ADC_ScanCpltCallback(adc_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}
                /* ADC_Thread entry function */
                /* pvParameters contains TaskHandle_t */
                void ADC_Task_entry(void * pvParameters)
                {
                    FSP_PARAMETER_NOT_USED(pvParameters);

                    (void) pressure_sensor_init();
                    soil_adc_init();
                    DS18B20_Init();

                    uint8_t uwt_cycle_div = 0U;
                    const TickType_t sample_period_ticks = pdMS_TO_TICKS(200U);
                    while(1)
                    {
                        sensor_fusion_update_jscope_time();

                        pressure_sample_t pressure_sample = {0};
                        uint32_t pressure_retry = 0U;
                        do
                        {
                            g_pressure_last_err = pressure_sensor_read(&pressure_sample);
                            if (FSP_SUCCESS == g_pressure_last_err)
                            {
                                break;
                            }
                            pressure_retry++;
                            vTaskDelay(pdMS_TO_TICKS(10U));
                        } while (pressure_retry < 2U);

                        if (FSP_SUCCESS == g_pressure_last_err)
                        {
                            g_pressure_latest = pressure_sample;
                            g_pressure_sample_count++;
                            g_jscope_pressure_kg_0 = pressure_sample.pressure_kg[0];
                            g_jscope_pressure_kg_1 = pressure_sample.pressure_kg[1];
                            g_jscope_pressure_kg_2 = pressure_sample.pressure_kg[2];
                        }

                        g_soil_adc_raw          = soil_adc_read_raw();
                        g_soil_moisture_percent = soil_adc_raw_to_percent(g_soil_adc_raw);
                        g_soil_sample_count++;
                        g_soil_sensor_state = (g_soil_adc_raw >= 4095U) ? 1U : 0U;
                        if (0U == g_soil_sensor_state)
                        {
                            g_soil_last_valid_raw = g_soil_adc_raw;
                        }

                        {
                            uint8_t start_threshold = g_soil_watering_threshold;
                            uint8_t stop_threshold =
                                (uint8_t) (((uint16_t) start_threshold + (uint16_t) g_soil_watering_hysteresis > 100U) ?
                                100U : ((uint16_t) start_threshold + (uint16_t) g_soil_watering_hysteresis));

                            if ((0U == g_soil_need_watering) && (g_soil_moisture_percent < start_threshold))
                            {
                                g_soil_need_watering = 1U;
                            }
                            else if ((0U != g_soil_need_watering) && (g_soil_moisture_percent > stop_threshold))
                            {
                                g_soil_need_watering = 0U;
                            }
                        }
                        g_jscope_soil_percent = g_soil_moisture_percent;

                        uwt_cycle_div++;
                        if ((uwt_cycle_div >= 5U) || (FSP_ERR_NOT_INITIALIZED == g_uwt_last_err))
                        {
                            uwt_cycle_div = 0U;
                            uint32_t uwt_retry = 0U;
                            do
                            {
                                (void) DS18B20_MeasureBlocking();
                                if (FSP_SUCCESS == g_uwt_last_err)
                                {
                                    break;
                                }
                                uwt_retry++;
                                vTaskDelay(pdMS_TO_TICKS(20U));
                            } while (uwt_retry < 2U);

                            g_uwt_retry_count = uwt_retry;
                            if (FSP_SUCCESS == g_uwt_last_err)
                            {
                                g_jscope_water_temp_c = g_uwt_temperature_c;
                            }
                        }

                        vTaskDelay(sample_period_ticks);
                    }
                }
