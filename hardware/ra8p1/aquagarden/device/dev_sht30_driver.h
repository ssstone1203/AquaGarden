#ifndef DEV_SHT30_DRIVER_H_
#define DEV_SHT30_DRIVER_H_

#include <stdbool.h>
#include "bsp_api.h"

void dev_sht30_init(void);

/* Non-blocking state machine aligned with hardware/demo/th_sensor:
 * periodic start 0x2130, then every ~1 s fetch 0xE000 + read 6 bytes.
 * Call from the 10 ms service task. */
void dev_sht30_process(void);

/* Returns true once at least one fetch completed; copies latest °C / %RH. */
bool dev_sht30_get(float * p_temp_c, float * p_humi_pct);

uint16_t dev_sht30_get_fail_count(void);

#endif /* DEV_SHT30_DRIVER_H_ */
