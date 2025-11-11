/*
 * dev_rgb_driver.h
 *
 *  Created on: 2025-11-11
 *      Author: davidwang
 */

#ifndef DEV_RGB_DRIVER_H_
#define DEV_RGB_DRIVER_H_

#include "common_data.h"
#include "hal_data.h"
#include "dev_rgb_xdefine.h"

typedef enum
{
    RGB_COLOR_RED,          //
    RGB_COLOR_GREEN,        //
    RGB_COLOR_BLUE,         //
    RGB_COLOR_YELLOW,       //
    RGB_COLOR_PURPLE,       //
    RGB_COLOR_CYAN,         //
    RGB_COLOR_WHITE,        //

}rgb_color_e;

typedef enum
{
    RGB_OBJECT_0 = 0,
    RGB_OBJECT_1,
    RGB_OBJECT_2,

}rgb_object_e;

typedef struct
{
    uint16_t rgb_bright_set;

}rgb_set_t;

void RGB_Init(rgb_object_e rgb_object);
void RGB_ColorBrightSet(rgb_set_t* rgb_set, rgb_object_e rgb_object,
                        rgb_color_e rgb_color, uint16_t bright_set);

#endif /* DEV_RGB_DRIVER_H_ */
