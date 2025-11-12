/*
 * dev_water_quality_sen.h
 *
 *  Created on: 2025年11月12日
 *      Author: davidwang
 */

#ifndef DEV_WATER_QUALITY_SEN_H_
#define DEV_WATER_QUALITY_SEN_H_

#include "hal_data.h"
#include "common_data.h"
#include "dev_water_quality_xdefine.h"

typedef struct
{
    uint8_t wqs_info_raw[27];
    float wqs_info_toc;
    float wqs_info_cod;
    float wqs_info_sd;  //色度
    float wqs_info_zd;  //浊度
    float wqs_info_temp;    //温度
    uint16_t wqs_info_tds;
    float wqs_info_uv254;
    uint16_t wqs_info_ec;  //电导率
    uint8_t wqs_info_wqi; //水质综合评分

}wqs_info_t;

typedef struct
{
    const uint8_t* wqs_cmd_detect;// = {0x55, 0xAA, 0x03, 0x0A, 0x00, 0xFE, 0x00, 0x00, 0x00, 0xFF};
    const uint8_t* wqs_cmd_calibrate;// = {0x55, 0xAA, 0x02, 0x0A, 0x00, 0x0A, 0x00, 0x00, 0x00, 0xFF};

}wqs_cmd_t;

typedef enum
{
    WQS_CMD_TYPE_DETECT,
    WQS_CMD_TYPE_CALIBRATE,

}wqs_cmd_type_e;

void WQS_Init(wqs_cmd_t* wqs_cmd);
void WQS_CmdSend(wqs_cmd_t* wqs_cmd, wqs_cmd_type_e wqs_cmd_type);
void WQS_InfoGet(wqs_info_t *wqs_info);

#endif /* DEV_WATER_QUALITY_SEN_H_ */
