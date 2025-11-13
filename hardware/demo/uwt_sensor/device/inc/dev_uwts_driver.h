/*
 * dev_uwts_driver.h
 *
 *  Created on: 2025年11月13日
 *      Author: davidwang
 */

#ifndef DEV_UWTS_DRIVER_H_
#define DEV_UWTS_DRIVER_H_

#include "hal_data.h"
#include "common_data.h"

#define UWTS_PIN_DQ     BSP_IO_PORT_00_PIN_13
#define UWTS_CMD_SKIP_ROM   0xCC    //忽略ROM
#define UWTS_CMD_READ_SCRATCHPAD    0xBE    //读暂存区
#define UWTS_CMD_TEMP_CONVERT   0x44       //开启温度转换

typedef struct
{
    float uwts_data_temp_real;
    uint16_t uwts_data_temp;
    uint8_t uwts_data_temp_l;
    uint8_t uwts_data_temp_h;
    bsp_io_level_t uwts_data_dq_level;
    uint8_t uwts_flag_presence;

}uwts_data_t;

void UWTS_Init(void);
void UWTS_Reset(uwts_data_t* uwts_data);
void UWTS_WriteByte(uint8_t data);
uint8_t UWTS_ReadByte(void);
void UWTS_ConversionStart(uwts_data_t* uwts_data);
void UWTS_TemperatureRead(uwts_data_t* uwts_data);

#endif /* DEV_UWTS_DRIVER_H_ */
