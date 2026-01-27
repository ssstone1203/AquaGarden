#include "WQS_Task.h"
/* WQS_Measure entry function */
/* pvParameters contains TaskHandle_t */
void WQS_Task_entry(void* pvParameters)
{
	FSP_PARAMETER_NOT_USED(pvParameters);

	/* TODO: add your own code here */
	while(1)
	{
		vTaskDelay(1);
	}
}

void WQS_UartCpltCallback(uart_callback_args_t* p_args)
{
	FSP_PARAMETER_NOT_USED(p_args);
}
