#ifndef DEV_SHT30_DRIVER_H_
#define DEV_SHT30_DRIVER_H_

#include <stdbool.h>
#include "bsp_api.h"

void dev_sht30_init(void);

/* Non-blocking state machine; call periodically (e.g. every 10 ms). The driver
 * self-paces a new single-shot measurement roughly once per second and never
 * blocks the caller. */
void dev_sht30_process(void);

/* Returns true and copies the latest valid temperature (C) / humidity (%RH)
 * once at least one successful measurement has completed. */
bool dev_sht30_get(float * p_temp_c, float * p_humi_pct);

uint16_t dev_sht30_get_fail_count(void);

#endif /* DEV_SHT30_DRIVER_H_ */
