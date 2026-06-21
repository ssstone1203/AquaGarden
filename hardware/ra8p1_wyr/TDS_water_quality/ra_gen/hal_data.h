/* generated HAL header file - do not edit */
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "r_adc_b.h"
                      #include "r_adc_api.h"
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
void hal_entry(void);
void g_hal_init(void);
FSP_FOOTER
#endif /* HAL_DATA_H_ */
