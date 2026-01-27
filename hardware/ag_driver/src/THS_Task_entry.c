#include "THS_Task.h"
/* THS_Measure entry function */
/* pvParameters contains TaskHandle_t */
void THS_Task_entry(void* pvParameters)
{
	FSP_PARAMETER_NOT_USED(pvParameters);

	/* TODO: add your own code here */
	while(1)
	{
		vTaskDelay(1);
	}
}

void THS_I2CMaster_CpltCallback(i2c_master_callback_args_t* p_args)
{
	FSP_PARAMETER_NOT_USED(p_args);;
}
