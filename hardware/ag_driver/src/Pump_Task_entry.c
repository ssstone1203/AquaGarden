#include "Pump_Task.h"
/* Pump_Driver entry function */
/* pvParameters contains TaskHandle_t */
void Pump_Task_entry(void* pvParameters)
{
	FSP_PARAMETER_NOT_USED(pvParameters);

	/* TODO: add your own code here */
	while(1)
	{
		vTaskDelay(1);
	}
}
