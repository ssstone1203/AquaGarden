#include "new_thread0.h"
#include "dev_tds_driver.h"

#include "FreeRTOS.h"
#include "task.h"

void new_thread0_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    tds_driver_init();

    for (;;)
    {
        tds_driver_process();
        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}
