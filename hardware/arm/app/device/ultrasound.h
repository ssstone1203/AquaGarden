#ifndef _ULTRASOUND_H_
#define _ULTRASOUND_H_

#include "hal_data.h"
#include "common_data.h"

#define ULTRASOUND_TRIG_PIN    BSP_IO_PORT_04_PIN_15
#define ULTRASOUND_ECHO_PIN    BSP_IO_PORT_04_PIN_14

void Ultrasound_Init(void);

/**
 * @brief  触发一次超声波测距并返回距离值（单位 cm）。
 * @return 距离值 (cm)；超时未收到回波时返回 -1.0f。
 */
float Ultrasound_GetDistance(void);

#endif
