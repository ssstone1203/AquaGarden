/* generated thread header file - do not edit */
#ifndef SENSOR_TASK_H_
#define SENSOR_TASK_H_
#include "bsp_api.h"
                #include "FreeRTOS.h"
                #include "task.h"
                #include "semphr.h"
                #include "hal_data.h"
                #ifdef __cplusplus
                extern "C" void Sensor_Task_entry(void * pvParameters);
                #else
                extern void Sensor_Task_entry(void * pvParameters);
                #endif
#include "r_adc_b.h"
                      #include "r_adc_api.h"
#include "r_iic_master.h"
#include "r_i2c_master_api.h"
FSP_HEADER
/** ADC on ADC_B instance. */
                    extern const adc_instance_t g_adc_b;

                    /** Access the ADC_B instance using these structures when calling API functions directly (::p_api is not used). */
                    extern adc_b_instance_ctrl_t g_adc_b_ctrl;
                    extern const adc_cfg_t g_adc_b_cfg;
                    extern const adc_b_scan_cfg_t g_adc_b_scan_cfg;

                    #ifndef NULL
                    void NULL(adc_callback_args_t * p_args);
                    #endif
/* I2C Master on IIC Instance. */
extern const i2c_master_instance_t g_i2c_master0;

/** Access the I2C Master instance using these structures when calling API functions directly (::p_api is not used). */
extern iic_master_instance_ctrl_t g_i2c_master0_ctrl;
extern const i2c_master_cfg_t g_i2c_master0_cfg;

#ifndef g_iic_master0_callback
void g_iic_master0_callback(i2c_master_callback_args_t * p_args);
#endif
FSP_FOOTER
#endif /* SENSOR_TASK_H_ */
