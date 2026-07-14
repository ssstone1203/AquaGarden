#include "pump_thread0.h"

#include "pump_drv8870.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

/* PWM only: pump_drv8870_set_power (GPT on IN2). */
#ifndef PUMP_PWM_PERCENT
#define PUMP_PWM_PERCENT       (40U)
#endif
/** Each “on” window (keep modest). */
#ifndef PUMP_ON_MS
#define PUMP_ON_MS             (2000U)
#endif
/** Idle between on windows. */
#ifndef PUMP_OFF_MS
#define PUMP_OFF_MS            (0U)
#endif
/** (抽一段 → 停一段) 重复次数 */
#ifndef PUMP_REPEAT_COUNT
#define PUMP_REPEAT_COUNT      (10U)
#endif

void pump_thread0_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    pump_drv8870_init();

    for (uint32_t i = 0U; i < (uint32_t) PUMP_REPEAT_COUNT; i++)
    {
        pump_drv8870_set_power((uint8_t) PUMP_PWM_PERCENT);
        vTaskDelay(pdMS_TO_TICKS(PUMP_ON_MS));

        pump_drv8870_stop();
        if ((i + 1U) < (uint32_t) PUMP_REPEAT_COUNT)
        {
            vTaskDelay(pdMS_TO_TICKS(PUMP_OFF_MS));
        }
    }

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}
