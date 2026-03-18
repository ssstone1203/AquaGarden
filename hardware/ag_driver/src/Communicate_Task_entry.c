#include "Communicate_Task.h"
/* Com_Thread entry function */
/* pvParameters contains TaskHandle_t */
void Communicate_Task_entry(void * pvParameters)
{
	FSP_PARAMETER_NOT_USED(pvParameters);

	/* TODO: add your own code here */
	while(1)
	{
		vTaskDelay(1);
	}
}

void Com_SPI_Callback(spi_callback_args_t * p_args)
{
	FSP_PARAMETER_NOT_USED(p_args);
}