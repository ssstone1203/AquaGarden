/* generated thread header file - do not edit */
#ifndef ADC_TASK_H_
#define ADC_TASK_H_
#include "bsp_api.h"
                #include "FreeRTOS.h"
                #include "task.h"
                #include "semphr.h"
                #include "hal_data.h"
                #ifdef __cplusplus
                extern "C" void ADC_Task_entry(void * pvParameters);
                #else
                extern void ADC_Task_entry(void * pvParameters);
                #endif
#include "r_adc.h"
#include "r_adc_api.h"
FSP_HEADER
/** ADC on ADC Instance. */
extern const adc_instance_t g_adc;

/** Access the ADC instance using these structures when calling API functions directly (::p_api is not used). */
extern adc_instance_ctrl_t g_adc_ctrl;
extern const adc_cfg_t g_adc_cfg;
extern const adc_channel_cfg_t g_adc_channel_cfg;

#ifndef ADC_ScanCpltCallback
void ADC_ScanCpltCallback(adc_callback_args_t * p_args);
#endif

#ifndef NULL
#define ADC_DMAC_CHANNELS_PER_BLOCK_NULL  0
#endif
FSP_FOOTER
#endif /* ADC_TASK_H_ */
