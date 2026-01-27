/* generated thread header file - do not edit */
#ifndef PUMP_TASK_H_
#define PUMP_TASK_H_
#include "bsp_api.h"
                #include "FreeRTOS.h"
                #include "task.h"
                #include "semphr.h"
                #include "hal_data.h"
                #ifdef __cplusplus
                extern "C" void Pump_Task_entry(void * pvParameters);
                #else
                extern void Pump_Task_entry(void * pvParameters);
                #endif
#include "r_gpt.h"
#include "r_timer_api.h"
FSP_HEADER
/** Timer on GPT Instance. */
extern const timer_instance_t g_pump_timer;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_pump_timer_ctrl;
extern const timer_cfg_t g_pump_timer_cfg;

#ifndef NULL
void NULL(timer_callback_args_t * p_args);
#endif
FSP_FOOTER
#endif /* PUMP_TASK_H_ */
