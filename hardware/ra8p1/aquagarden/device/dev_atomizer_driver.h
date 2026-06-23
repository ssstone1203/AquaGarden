#ifndef DEV_ATOMIZER_DRIVER_H_
#define DEV_ATOMIZER_DRIVER_H_

#include <stdint.h>
#include "bsp_api.h"

void dev_atomizer_init(void);
fsp_err_t dev_atomizer_set(uint8_t state);
uint8_t dev_atomizer_get(void);

#endif /* DEV_ATOMIZER_DRIVER_H_ */
