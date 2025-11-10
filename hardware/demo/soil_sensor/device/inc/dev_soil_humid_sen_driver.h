/*
 * dev_soil_humid_sen_driver.h
 *
 *  Created on: 2025-11-11
 *      Author: davidwang
 */

#ifndef DEV_SOIL_HUMID_SEN_DRIVER_H_
#define DEV_SOIL_HUMID_SEN_DRIVER_H_

#include "hal_data.h"
#include "common_data.h"

typedef struct
{
    uint16_t shs_data_raw;

}shs_data_t;

extern shs_data_t g_shs_data;

void SHS_Init(void);
void SHS_ScanStart(void);
void SHS_ScanStop(void);
void SHS_ScanCpltCallback(adc_callback_args_t *p_args);

#endif /* DEV_SOIL_HUMID_SEN_DRIVER_H_ */
