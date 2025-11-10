/*
 * dev_press_sen_driver.c
 *
 *  Created on: 2025-11-10
 *      Author: davidwang
 */
#include "dev_press_sen_driver.h"

void PS_Init(void)
{
    R_ADC_Open(&g_ps_adc_ctrl, &g_ps_adc_cfg);
    R_ADC_ScanCfg(&g_ps_adc_ctrl, &g_ps_adc_channel_cfg);
}

void PS_ScanStart(void)
{
    R_ADC_ScanStart(&g_ps_adc_ctrl);
}

void PS_ScanStop(void)
{
    R_ADC_ScanStop(&g_ps_adc_ctrl);
}

static uint16_t g_ps_data_raw_temp[PS_NUM];
void PS_ScanCpltCallback(adc_callback_args_t *p_args)
{
    R_ADC_Read(&g_ps_adc_ctrl, ADC_CHANNEL_2, &g_ps_data_raw_temp[PS_MEDIUM]);
}

void PS_DataRawGet(ps_object_e ps_object, ps_data_t ps_data[PS_NUM])
{
    switch(ps_object)
    {
        case PS_MEDIUM:
        {
            ps_data[PS_MEDIUM].ps_data_raw = g_ps_data_raw_temp[PS_MEDIUM];
            break;
        }
        default:
            break;
    }
}

#ifdef DEBUG
void PS_Init(ps_adc_ctrl_t* ps_adc_ctrl, ps_adc_cfg_t* ps_adc_cfg,
             ps_channel_cfg_t* ps_channel_cfg)
{
    if(ps_adc_ctrl != NULL)
    {
        free(ps_adc_ctrl);
    }
    ps_adc_ctrl = (ps_adc_ctrl_t*)malloc(sizeof(ps_adc_ctrl_t));
    ps_adc_ctrl = &g_ps_adc_ctrl;

    if(ps_adc_cfg != NULL)
    {
        free(ps_adc_cfg);
    }
    ps_adc_cfg = (ps_adc_cfg_t*)malloc(sizeof(ps_adc_cfg_t));
    ps_adc_cfg = &g_ps_adc_cfg;

    if(ps_channel_cfg != NULL)
    {
        free(ps_channel_cfg);
    }
    ps_channel_cfg = (ps_channel_cfg_t*)malloc(sizeof(ps_channel_cfg_t));
    ps_channel_cfg = &g_ps_adc_channel_cfg;

    R_ADC_Open(ps_adc_ctrl, ps_adc_cfg);
    R_ADC_ScanCfg(ps_adc_ctrl, ps_channel_cfg);
}

void PS_ScanStart(ps_adc_ctrl_t* ps_adc_ctrl)
{
    R_ADC_ScanStart(ps_adc_ctrl);
}

void PS_ScanStop(ps_adc_ctrl_t* ps_adc_ctrl)
{
    R_ADC_ScanStop(ps_adc_ctrl);
}

void PS_DataGet(ps_adc_ctrl_t* ps_adc_ctrl, ps_object_e ps_object, ps_data_t ps_data)
{
    switch(ps_object)
    {
        case PS_MEDIUM:
        {
            R_ADC_Read(ps_adc_ctrl, ADC_CHANNEL_2, &ps_data.ps_data_raw[PS_MEDIUM]);
            break;
        }
        default:
            break;
    }
}
#endif
