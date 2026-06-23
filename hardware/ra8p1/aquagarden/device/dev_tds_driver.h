#ifndef DEV_TDS_DRIVER_H_
#define DEV_TDS_DRIVER_H_

#include <stdint.h>
#include "bsp_api.h"

void dev_tds_init(void);
fsp_err_t dev_tds_read(uint16_t * p_raw, float * p_voltage, uint16_t * p_ntu);

#endif /* DEV_TDS_DRIVER_H_ */
