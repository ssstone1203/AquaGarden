#ifndef DEV_ADC_SHARED_H_
#define DEV_ADC_SHARED_H_

#include <stdbool.h>
#include "bsp_api.h"

/* Shared by soil (AN001) and TDS (AN002). Bring-up mirrors
 * hardware/demo/soil_sensor Open+ScanCfg+ScanStart, plus RA8P1 ADC_B calibrate. */
void dev_adc_shared_init(void);
bool dev_adc_shared_is_ready(void);

#endif /* DEV_ADC_SHARED_H_ */
