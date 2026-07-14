/**
 * USB alarm light – 直接发送 usb_light需求.md 中已验证的 8 字节指令。
 */
#ifndef USB_LIGHT_H_
#define USB_LIGHT_H_

#include "bsp_api.h"
#include "ch340_host.h"

/** 与需求文档条目一一对应（每条 8 字节 HEX 已内置）。 */
typedef enum e_usb_light_mode
{
    USB_LIGHT_RED_ON = 0,
    USB_LIGHT_YELLOW_ON,
    USB_LIGHT_GREEN_ON,
    USB_LIGHT_RED_ON_HORN,
    USB_LIGHT_WHITE_ON,
    USB_LIGHT_CYAN_ON,
    USB_LIGHT_PURPLE_ON,
    USB_LIGHT_BLUE_ON,

    USB_LIGHT_RED_SLOW,
    USB_LIGHT_YELLOW_SLOW,
    USB_LIGHT_GREEN_SLOW,
    USB_LIGHT_RED_SLOW_HORN,
    USB_LIGHT_WHITE_SLOW,
    USB_LIGHT_CYAN_SLOW,
    USB_LIGHT_PURPLE_SLOW,
    USB_LIGHT_BLUE_SLOW,

    USB_LIGHT_RED_FAST,
    USB_LIGHT_YELLOW_FAST,
    USB_LIGHT_GREEN_FAST,
    USB_LIGHT_RED_FAST_HORN,
    USB_LIGHT_WHITE_FAST,
    USB_LIGHT_CYAN_FAST,
    USB_LIGHT_PURPLE_FAST,
    USB_LIGHT_BLUE_FAST,

    USB_LIGHT_HORN_ON,
    USB_LIGHT_HORN_OFF,
    USB_LIGHT_OFF,
} usb_light_mode_t;

fsp_err_t usb_light_set(usb_light_mode_t mode);

/** Last 8 bytes queued for Bulk OUT (compare with serial tool HEX). */
extern volatile uint8_t g_usb_light_last_frame[CH340_HOST_FRAME_LEN];

#endif /* USB_LIGHT_H_ */
