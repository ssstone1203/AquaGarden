#ifndef DEV_USB_LIGHT_DRIVER_H_
#define DEV_USB_LIGHT_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>
#include "bsp_api.h"

#define DEV_USB_LIGHT_MODE_OFF (26U)

void dev_usb_light_init(void);
void dev_usb_light_process(void);
fsp_err_t dev_usb_light_set_mode(uint8_t mode);
bool dev_usb_light_ready(void);
uint8_t dev_usb_light_get_mode(void);
fsp_err_t dev_usb_light_get_last_error(void);

#endif /* DEV_USB_LIGHT_DRIVER_H_ */
