#include "WQS_Task.h"
#include "wqs_sensor.h"
#include "sensor_fusion.h"
                /* WQS_Measure entry function */
                /* pvParameters contains TaskHandle_t */
                void WQS_Task_entry(void * pvParameters)
                {
                    FSP_PARAMETER_NOT_USED(pvParameters);
                    wqs_cmd_t cmd = {0};
                    WQS_Init(&cmd);

                    const TickType_t sample_period_ticks = pdMS_TO_TICKS(1500U);
                    while(1)
                    {
                        bool ok = false;
                        uint32_t retry_count = 0U;
                        do
                        {
                            WQS_CmdSend(&cmd, WQS_CMD_TYPE_DETECT);
                            ok = WQS_InfoGet(&g_wqs_info);
                            if (ok)
                            {
                                break;
                            }
                            retry_count++;
                            vTaskDelay(pdMS_TO_TICKS(100U));
                        } while (retry_count < 2U);

                        g_wqs_retry_count = retry_count;
                        g_wqs_last_read_ok = ok ? 1U : 0U;
                        g_wqs_measure_count++;
                        g_wqs_last_err = ok ? FSP_SUCCESS : FSP_ERR_INVALID_DATA;
                        if (ok)
                        {
                            g_jscope_wqs_wqi = (float) g_wqs_info.wqs_info_wqi;
                        }

                        sensor_fusion_update_jscope_time();
                        vTaskDelay(sample_period_ticks);
                    }
                }
