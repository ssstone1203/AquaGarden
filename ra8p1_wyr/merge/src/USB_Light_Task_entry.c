#include "USB_Light_Task.h"
#include "usb_app.h"
#include "app_startup.h"
#include "sensor_fusion.h"

#include "FreeRTOS.h"
#include "task.h"

/** Same flow as usb_light/src/hal_entry.c: init once, then usb_app_process() forever. */
void USB_Light_Task_entry(void * pvParameters)
{
    bool usb_initialized = false;

    FSP_PARAMETER_NOT_USED(pvParameters);

    while (!usb_initialized)
    {
        g_task_hb_usb++;

        if (FSP_SUCCESS == usb_app_init())
        {
            usb_initialized = true;
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(200U));
        }
    }

    for (;;)
    {
        g_task_hb_usb++;

        if (0U == g_usb_light_enable)
        {
            vTaskDelay(pdMS_TO_TICKS(50U));
            continue;
        }

        usb_app_process();
    }
}
