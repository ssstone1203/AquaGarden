#ifndef DEV_SOIL_HUMID_SEN_DRIVER_H_
#define DEV_SOIL_HUMID_SEN_DRIVER_H_

#include <stdint.h>
#include "bsp_api.h"

void dev_soil_init(void);
fsp_err_t dev_soil_read(uint16_t * p_raw, uint8_t * p_percent);

#endif /* DEV_SOIL_HUMID_SEN_DRIVER_H_ */
