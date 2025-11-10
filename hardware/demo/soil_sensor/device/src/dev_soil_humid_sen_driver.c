/*
 * dev_soil_humid_sen_driver.c
 *
 *  Created on: 2025-11-11
 *      Author: davidwang
 */
#include "dev_soil_humid_sen_driver.h"

void SHS_Init(void)
{
    R_ADC_Open(&g_shs_adc_ctrl, &g_shs_adc_cfg);
    R_ADC_ScanCfg(&g_shs_adc_ctrl, &g_shs_adc_channel_cfg);
}

void SHS_ScanStart(void)
{
    R_ADC_ScanStart(&g_shs_adc_ctrl);
}

void SHS_ScanStop(void)
{
    R_ADC_ScanStop(&g_shs_adc_ctrl);
}

#if 1
shs_data_t g_shs_data;
uint16_t g_data;
void SHS_ScanCpltCallback(adc_callback_args_t *p_args)
{
    R_ADC_Read(&g_shs_adc_ctrl, ADC_CHANNEL_1, &g_shs_data.shs_data_raw);
    g_data = g_shs_data.shs_data_raw;   //debug
}

#endif
