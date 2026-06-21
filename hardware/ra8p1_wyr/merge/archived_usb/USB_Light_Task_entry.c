#include "USB_Light_Task.h"
#include "usb_app.h"
#include "usb_light.h"
#include "sensor_fusion.h"
#include "app_startup.h"

#include "FreeRTOS.h"
#include "task.h"

void USB_Light_Task_entry(void * pvParameters)
{
    uint8_t last_mode = 0xFFU;

    FSP_PARAMETER_NOT_USED(pvParameters);

    for (;;)
    {
        g_task_hb_usb++;

        if (0U == g_usb_light_enable)
        {
            last_mode = 0xFFU;
            vTaskDelay(pdMS_TO_TICKS(50U));
            continue;
        }

        if (0U == usb_app_is_ready())
        {
            if (FSP_SUCCESS != usb_app_init())
            {
                vTaskDelay(pdMS_TO_TICKS(200U));
                continue;
            }
        }

        usb_app_poll();

        if (g_usb_light_mode_request != last_mode)
        {
            if (g_usb_light_mode_request < USB_LIGHT_OFF)
            {
                (void) usb_light_set((usb_light_mode_t) g_usb_light_mode_request);
            }
            else if (USB_LIGHT_OFF == g_usb_light_mode_request)
            {
                (void) usb_light_set(USB_LIGHT_OFF);
            }

            last_mode = g_usb_light_mode_request;
        }

        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}
