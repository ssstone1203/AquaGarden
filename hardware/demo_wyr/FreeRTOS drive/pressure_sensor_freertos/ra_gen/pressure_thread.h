/* generated thread header file - do not edit */
#ifndef PRESSURE_THREAD_H_
#define PRESSURE_THREAD_H_
#include "bsp_api.h"
                #include "FreeRTOS.h"
                #include "task.h"
                #include "semphr.h"
                #include "hal_data.h"
                #ifdef __cplusplus
                extern "C" void pressure_thread_entry(void * pvParameters);
                #else
                extern void pressure_thread_entry(void * pvParameters);
                #endif
FSP_HEADER
FSP_FOOTER
#endif /* PRESSURE_THREAD_H_ */
