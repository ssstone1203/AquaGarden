#ifndef DS18B20_H_
#define DS18B20_H_

#include "bsp_api.h"
#include <stdint.h>

#define DS18B20_TEMP_MIN_C    (-55.0f)
#define DS18B20_TEMP_MAX_C    (125.0f)

/** Keil / 调试器 Watch：最近一次读数（仅当 g_uwt_last_err == FSP_SUCCESS 有效） */
extern volatile float g_uwt_temperature_c;

/** 原始温度字，单位 1/16 °C（补码） */
extern volatile int16_t g_uwt_temperature_raw;

/** 最近一次操作错误码 */
extern volatile fsp_err_t g_uwt_last_err;

void      DS18B20_Init(void);
fsp_err_t DS18B20_ConvertT_Start(void);

/** 假定转换已完成：读暂存器并更新全局温度/原始字 */
fsp_err_t DS18B20_ReadResult(void);

/** 忙等转换完成再读（无 RTOS 或测试用），并更新全局变量 */
fsp_err_t DS18B20_MeasureBlocking(void);

#endif /* DS18B20_H_ */
