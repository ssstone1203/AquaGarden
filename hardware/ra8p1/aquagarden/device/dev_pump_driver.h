#ifndef DEV_PUMP_DRIVER_H_
#define DEV_PUMP_DRIVER_H_

#include <stdint.h>
#include "bsp_api.h"

void dev_pump_init(void);
fsp_err_t dev_pump_set_pwm(uint8_t pwm_percent);
uint8_t dev_pump_get_pwm(void);

#endif /* DEV_PUMP_DRIVER_H_ */
