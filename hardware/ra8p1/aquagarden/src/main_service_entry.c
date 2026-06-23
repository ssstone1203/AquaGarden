#include "main_service.h"
#include "../module/aquagarden_app.h"
#include "../module/com_service.h"

/* Required by FreeRTOS because configCHECK_FOR_STACK_OVERFLOW is enabled.
 * On overflow, halt so the fault can be caught by a debugger. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char * pcTaskName)
{
    FSP_PARAMETER_NOT_USED(xTask);
    FSP_PARAMETER_NOT_USED(pcTaskName);

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        /* Trap. */
    }
}

/* main_thread entry: single 10 ms service task.
 * Runs sensor state machines, sensor fusion, actuator control, USB Host polling
 * and the UART up/down link. No second task is used. */
void main_service_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    aqua_app_init();
    com_service_init();

    for (;;)
    {
        aqua_app_process_10ms();
        com_service_process_10ms();
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}
