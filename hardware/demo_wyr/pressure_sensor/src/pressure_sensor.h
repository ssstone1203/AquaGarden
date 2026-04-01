#ifndef PRESSURE_SENSOR_H_
#define PRESSURE_SENSOR_H_

#include "hal_data.h"

/* 本示例默认接了 3 个压力传感器：AN000 / AN001 / AN002 */
#define PRESSURE_SENSOR_NUM        (3U)

/* 方便在调试窗口中直接观察三个通道的原始值与压力值 */
extern uint16_t g_adc_raw[PRESSURE_SENSOR_NUM];
extern float    g_pressure[PRESSURE_SENSOR_NUM];

fsp_err_t pressure_sensor_init(void);

/* 读取指定通道（0/1/2），返回 ADC 原始值和压力值 */
fsp_err_t pressure_sensor_read_one(uint8_t index,
                                   uint16_t * p_adc_raw,
                                   float * p_pressure_kg);

/* 一次性读取三个通道的值，数组长度必须为 PRESSURE_SENSOR_NUM */
fsp_err_t pressure_sensor_read_all(uint16_t adc_raw[PRESSURE_SENSOR_NUM],
                                   float pressure_kg[PRESSURE_SENSOR_NUM]);

float     pressure_sensor_convert_to_kg(uint16_t adc_raw);

#endif /* PRESSURE_SENSOR_H_ */

