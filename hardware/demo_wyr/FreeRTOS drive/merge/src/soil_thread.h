#ifndef SOIL_THREAD_H_
#define SOIL_THREAD_H_

#include "bsp_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "hal_data.h"

void soil_thread_entry(void * pvParameters);

#endif /* SOIL_THREAD_H_ */
