#ifndef _RGB_H_
#define _RGB_H_

#include "hal_data.h"

#define WS2812_NUM_LEDS   12

void RGB_Init(void);
void RGB_SetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void RGB_SetAll(uint8_t r, uint8_t g, uint8_t b);
void RGB_Clear(void);
void RGB_Show(void);
void RGB_SetBrightness(uint8_t brightness);

#endif
