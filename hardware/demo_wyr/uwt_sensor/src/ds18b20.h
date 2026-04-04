/* DS18B20 1-Wire temperature sensor (datasheet: -55 °C ~ +125 °C, 0.0625 °C steps @ 12-bit). */
#ifndef DS18B20_H_
#define DS18B20_H_

#include "bsp_api.h"
#include <stdbool.h>
#include <stdint.h>

/** DQ pin: FSP 引脚名 UWS_DQ → P409 (与 ra_gen/pin_data.c 一致). */
#define DS18B20_DQ_PIN    (BSP_IO_PORT_04_PIN_09)

/** 手册标称测温范围 (°C)，供显示/逻辑钳位. */
#define DS18B20_TEMP_MIN_C    (-55.0f)
#define DS18B20_TEMP_MAX_C    (125.0f)

void      DS18B20_Init(void);

/**
 * 发起一次温度转换并读暂存器，输出摄氏度浮点；可选返回原始 16 位温度字。
 *
 * 数据含义（与手册一致）:
 * - 传感器温度字为 16 位二进制补码，单位为 1/16 °C（12 位分辨率时步进 0.0625 °C）。
 * - 本函数将原始字换算为 float 摄氏度并钳位到 [DS18B20_TEMP_MIN_C, DS18B20_TEMP_MAX_C]。
 *
 * @param[out] p_temp_c   摄氏度 (°C)，可为 NULL 仅做总线/CRC 自检。
 * @param[out] p_raw_opt  原始温度寄存器 (LSB/MSB 拼成 int16)，Watch 窗口可看十进制/十六进制；可为 NULL。
 */
fsp_err_t DS18B20_ReadTemperatureC(float * p_temp_c, int16_t * p_raw_opt);

#endif                                 /* DS18B20_H_ */
