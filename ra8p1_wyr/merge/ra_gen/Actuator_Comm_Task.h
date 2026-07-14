/* generated thread header file - do not edit */
#ifndef ACTUATOR_COMM_TASK_H_
#define ACTUATOR_COMM_TASK_H_
#include "bsp_api.h"
                #include "FreeRTOS.h"
                #include "task.h"
                #include "semphr.h"
                #include "hal_data.h"
                #ifdef __cplusplus
                extern "C" void Actuator_Comm_Task_entry(void * pvParameters);
                #else
                extern void Actuator_Comm_Task_entry(void * pvParameters);
                #endif
#include "r_sci_b_uart.h"
            #include "r_uart_api.h"
#include "r_gpt.h"
#include "r_timer_api.h"
FSP_HEADER
/** UART on SCI Instance. */
            extern const uart_instance_t      g_com_uart0;

            /** Access the UART instance using these structures when calling API functions directly (::p_api is not used). */
            extern sci_b_uart_instance_ctrl_t     g_com_uart0_ctrl;
            extern const uart_cfg_t g_com_uart0_cfg;
            extern const sci_b_uart_extended_cfg_t g_com_uart0_cfg_extend;

            #ifndef UART_Rx_Callback
            void UART_Rx_Callback(uart_callback_args_t * p_args);
            #endif
/** Timer on GPT Instance. */
extern const timer_instance_t g_timer0;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_timer0_ctrl;
extern const timer_cfg_t g_timer0_cfg;

#ifndef NULL
void NULL(timer_callback_args_t * p_args);
#endif
FSP_FOOTER
#endif /* ACTUATOR_COMM_TASK_H_ */
