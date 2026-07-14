#include "new_thread0.h"

#include "ds18b20.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef UWT_CONVERT_WAIT_MS
#define UWT_CONVERT_WAIT_MS    (800U)
#endif
#ifndef UWT_SAMPLE_PERIOD_MS
#define UWT_SAMPLE_PERIOD_MS   (2000U)
#endif

void new_thread0_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    DS18B20_Init();

    for (;;)
    {
        if (FSP_SUCCESS == DS18B20_ConvertT_Start())
        {
            vTaskDelay(pdMS_TO_TICKS(UWT_CONVERT_WAIT_MS));
            (void) DS18B20_ReadResult();
        }
        vTaskDelay(pdMS_TO_TICKS(UWT_SAMPLE_PERIOD_MS));
    }
}
