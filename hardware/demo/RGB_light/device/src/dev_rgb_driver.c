/*
 * dev_rgb_driver.c
 *
 *  Created on: 2025-11-11
 *      Author: davidwang
 */

#include "dev_rgb_driver.h"

void RGB_Init(rgb_object_e rgb_object)
{
    switch (rgb_object)
    {
        case RGB_OBJECT_0:
        {
            R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
            R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
            R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_LOW);
            break;
        }
        case RGB_OBJECT_1:
        {
            R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
            R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
            R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_LOW);
            break;
        }
        case RGB_OBJECT_2:
        {
            R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
            R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
            R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_LOW);
            break;
        }
        default:
        break;
    }
}

void RGB_ColorBrightSet(rgb_set_t* rgb_set, rgb_object_e rgb_object,
                        rgb_color_e rgb_color, uint16_t bright_set)
{
    switch (rgb_object)
    {
        case RGB_OBJECT_0:
        {
            rgb_set[RGB_OBJECT_0].rgb_bright_set = bright_set;
            switch (rgb_color)
            {
                case RGB_COLOR_RED:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_GREEN:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_BLUE:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_PURPLE:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_CYAN:    //青色
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_WHITE:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_YELLOW:
                {
                    if (rgb_set[RGB_OBJECT_0].rgb_bright_set < 5 && rgb_set[RGB_OBJECT_0].rgb_bright_set != 0)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (4, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (1, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_0].rgb_bright_set == 6)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (1, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (4, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_0].rgb_bright_set == 7)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (3, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_0].rgb_bright_set == 8)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (6, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_0].rgb_bright_set == 9)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (7, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (1, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_0].rgb_bright_set == 10)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (8, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (0, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_0].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                default:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT0_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    break;
                }
            }
            break;
        }
        case RGB_OBJECT_1:
        {
            rgb_set[RGB_OBJECT_1].rgb_bright_set = bright_set;
            switch (rgb_color)
            {
                case RGB_COLOR_RED:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_GREEN:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_BLUE:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_PURPLE:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_CYAN:    //青色
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_WHITE:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_YELLOW:
                {
                    if (rgb_set[RGB_OBJECT_1].rgb_bright_set < 5 && rgb_set[RGB_OBJECT_1].rgb_bright_set != 0)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (4, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (1, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_1].rgb_bright_set == 6)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (1, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (4, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_1].rgb_bright_set == 7)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (3, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_1].rgb_bright_set == 8)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (6, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_1].rgb_bright_set == 9)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (7, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (1, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_1].rgb_bright_set == 10)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (8, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (0, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_1].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                default:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT1_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    break;
                }
            }
            break;
        }
        case RGB_OBJECT_2:
        {
            rgb_set[RGB_OBJECT_2].rgb_bright_set = bright_set;
            switch (rgb_color)
            {
                case RGB_COLOR_RED:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_GREEN:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_BLUE:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_PURPLE:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_CYAN:    //青色
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_WHITE:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                case RGB_COLOR_YELLOW:
                {
                    if (rgb_set[RGB_OBJECT_2].rgb_bright_set < 5 && rgb_set[RGB_OBJECT_2].rgb_bright_set != 0)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (4, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (1, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_2].rgb_bright_set == 6)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (1, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (4, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_2].rgb_bright_set == 7)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (3, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_2].rgb_bright_set == 8)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (6, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_2].rgb_bright_set == 9)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (7, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (1, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    else if (rgb_set[RGB_OBJECT_2].rgb_bright_set == 10)
                    {
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (8, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                        R_BSP_SoftwareDelay (2, BSP_DELAY_UNITS_MILLISECONDS);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                        R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                        R_BSP_SoftwareDelay (0, BSP_DELAY_UNITS_MILLISECONDS);
                    }
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_HIGH);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_HIGH);
                    R_BSP_SoftwareDelay (rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_BSP_SoftwareDelay (10 - rgb_set[RGB_OBJECT_2].rgb_bright_set, BSP_DELAY_UNITS_MILLISECONDS);
                    break;
                }
                default:
                {
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_RED, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_GREEN, BSP_IO_LEVEL_LOW);
                    R_IOPORT_PinWrite (&g_ioport_ctrl, RGB_OBJECT2_PIN_BLUE, BSP_IO_LEVEL_LOW);
                    break;
                }
            }
            break;
        }
        default:
        break;
    }
}
