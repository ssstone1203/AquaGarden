#include "THS_Task.h"
#include "sht30.h"
#include "sensor_fusion.h"
                /* THS_Measure entry function */
                /* pvParameters contains TaskHandle_t */
                void THS_Task_entry(void * pvParameters)
                {
                    FSP_PARAMETER_NOT_USED(pvParameters);
                    /* Avoid being starved by higher-priority compute-heavy loops during bring-up. */
                    vTaskPrioritySet(NULL, 2U);

                    bool sht30_ready = false;
                    fsp_err_t err_init = FSP_ERR_NOT_INITIALIZED;

                    const TickType_t sample_period_ticks = pdMS_TO_TICKS(1000U);
                    while(1)
                    {
                        if (!sht30_ready)
                        {
                            err_init = sht30_init(&g_ths_i2c_master, SHT30_I2C_ADDR_7BIT_DEFAULT);
                            if ((FSP_SUCCESS == err_init) || (FSP_ERR_ALREADY_OPEN == err_init))
                            {
                                sht30_ready = true;
                            }
                            else
                            {
                                /* Keep retrying open so startup order or bus glitches won't leave the task in NOT_INITIALIZED forever. */
                                g_air_last_err = err_init;
                                sensor_fusion_update_jscope_time();
                                vTaskDelay(pdMS_TO_TICKS(200U));
                                continue;
                            }
                        }

                        float temp_c = 0.0F;
                        float rh     = 0.0F;
                        uint32_t retry_count = 0U;
                        do
                        {
                            err_init = sht30_measure_single_shot(&temp_c, &rh, NULL, true);
                            if (FSP_SUCCESS == err_init)
                            {
                                break;
                            }
                            retry_count++;
                            vTaskDelay(pdMS_TO_TICKS(20U));
                        } while (retry_count < 2U);

                        g_air_retry_count = retry_count;
                        g_air_last_err = err_init;
                        if (FSP_SUCCESS == err_init)
                        {
                            g_jscope_air_temp_c     = temp_c;
                            g_jscope_air_humidity_rh = rh;
                        }
                        else
                        {
                            /* If the underlying I2C instance gets reset/closed, force re-open path next loop. */
                            if ((FSP_ERR_NOT_INITIALIZED == err_init) || (FSP_ERR_NOT_OPEN == err_init))
                            {
                                sht30_ready = false;
                            }
                        }

                        sensor_fusion_update_jscope_time();
                        vTaskDelay(sample_period_ticks);
                    }
                }
