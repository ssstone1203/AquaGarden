/*
 * dev_press_sen_driver.h
 *
 *  Created on: 2025-11-10
 *      Author: davidwang
 */

#ifndef DEV_PRESS_SEN_DRIVER_H_
#define DEV_PRESS_SEN_DRIVER_H_

#include "hal_data.h"
#include "common_data.h"
#include "dev_press_sen_xdefine.h"

typedef struct
{
    uint16_t ps_data_raw;

}ps_data_t;

typedef enum
{
    PS_MEDIUM = 0,

}ps_object_e;

void PS_Init(void);
void PS_ScanStart(void);
void PS_ScanStop(void);
void PS_DataRawGet(ps_object_e ps_object, ps_data_t ps_data[PS_NUM]);
void PS_ScanCpltCallback(adc_callback_args_t *p_args);

#ifdef DEBUG

typedef adc_instance_ctrl_t ps_adc_ctrl_t;
typedef adc_cfg_t ps_adc_cfg_t;
typedef adc_channel_cfg_t ps_channel_cfg_t;

typedef enum
{
    PS_MEDIUM = 0,

}ps_object_e;

typedef struct
{
    uint16_t ps_data_raw[PS_NUM];

}ps_data_t;

void PS_Init(ps_adc_ctrl_t* ps_adc_ctrl, ps_adc_cfg_t* ps_adc_cfg,
             ps_channel_cfg_t* ps_channel_cfg);
void PS_ScanStart(ps_adc_ctrl_t* ps_adc_ctrl);
void PS_ScanStop(ps_adc_ctrl_t* ps_adc_ctrl);
void PS_DataGet(ps_adc_ctrl_t* ps_adc_ctrl, ps_object_e ps_object, ps_data_t ps_data);

void PS_ScanCpltCallback(adc_callback_args_t *p_args);

#endif

#endif /* DEV_PRESS_SEN_DRIVER_H_ */
