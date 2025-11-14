/*
 * dev_ths_driver.h
 *
 *  Created on: 2025年11月14日
 *      Author: davidwang
 */

#ifndef DEV_THS_DRIVER_H_
#define DEV_THS_DRIVER_H_

#include "hal_data.h"
#include"common_data.h"

#define THS_CMD_SOFT_RESET  0x30A2

#define THS_CMD_PERIODIC_DATA_FETCH  0xE000
#define THS_CMD_PERIODIC_STOP   0x3093

//high repeatability
#define THS_CMD_PERIODIC_H_MPS_1  0x2130
#define THS_CMD_PERIODIC_H_MPS_2  0x2236
#define THS_CMD_PERIODIC_H_MPS_4  0x2334
#define THS_CMD_PERIODIC_H_MPS_10 0x2737

typedef enum
{
    THS_REPEAT_TYPE_HIGH,
    THS_REPEAT_TYPE_MEDIUM,
    THS_REPEAT_TYPE_LOW,

}ths_repeat_type_e;

typedef struct
{
    uint16_t ths_cmd_raw;
    uint8_t ths_cmd_byte[2];

}ths_cmd_t;

typedef struct
{
    uint8_t ths_data_raw[6];
    float ths_data_temp;
    uint16_t ths_data_temp_raw;
    float ths_data_rh;  //relative humidity
    uint16_t ths_data_rh_raw;

}ths_data_t;

void THS_PeriodicModeStart(ths_cmd_t* ths_cmd, ths_repeat_type_e ths_repeat_type);
void THS_PeriodicDataRead(ths_cmd_t *ths_cmd, ths_data_t *ths_data);
void THS_PeriodicModeStop(ths_cmd_t* ths_cmd);

#endif /* INC_DEV_THS_DRIVER_H_ */
