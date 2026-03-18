#include "ADC_Task.h"
/* ADC_Thread entry function */
/* pvParameters contains TaskHandle_t */
void ADC_Task_entry(void * pvParameters)
{
	FSP_PARAMETER_NOT_USED(pvParameters);

	/* TODO: add your own code here */
	while(1)
	{
		vTaskDelay(1);
	}
}

void ADC_ScanCpltCallback(adc_callback_args_t * p_args)
{
	FSP_PARAMETER_NOT_USED(p_args);
}