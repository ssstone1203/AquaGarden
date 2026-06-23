#ifndef DEV_DS18B20_DRIVER_H_
#define DEV_DS18B20_DRIVER_H_

#include "bsp_api.h"

void dev_ds18b20_init(void);
fsp_err_t dev_ds18b20_measure(float * p_temp_c);

#endif /* DEV_DS18B20_DRIVER_H_ */
