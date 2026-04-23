/* generated thread header file - do not edit */
#ifndef THS_TASK_H_
#define THS_TASK_H_
#include "bsp_api.h"
                #include "FreeRTOS.h"
                #include "task.h"
                #include "semphr.h"
                #include "hal_data.h"
                #ifdef __cplusplus
                extern "C" void THS_Task_entry(void * pvParameters);
                #else
                extern void THS_Task_entry(void * pvParameters);
                #endif
#include "r_iic_b_master.h"
#include "r_i2c_master_api.h"
FSP_HEADER
/* I2C Master on IIC Instance. */
extern const i2c_master_instance_t g_ths_i2c_master;

/** Access the I2C Master instance using these structures when calling API functions directly (::p_api is not used). */
extern iic_b_master_instance_ctrl_t g_ths_i2c_master_ctrl;
extern const i2c_master_cfg_t g_ths_i2c_master_cfg;

#ifndef THS_I2CMaster_CpltCallback
void THS_I2CMaster_CpltCallback(i2c_master_callback_args_t * p_args);
#endif
FSP_FOOTER
#endif /* THS_TASK_H_ */
