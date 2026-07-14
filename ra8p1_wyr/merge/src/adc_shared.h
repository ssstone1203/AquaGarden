#ifndef ADC_SHARED_H_
#define ADC_SHARED_H_

#include <stdint.h>
#include "bsp_api.h"

void      adc_shared_init(void);
fsp_err_t adc_shared_read(uint8_t channel, uint16_t * raw_out);

extern volatile uint8_t   g_adc_shared_ready;
extern volatile uint8_t   g_adc_shared_init_step;
extern volatile fsp_err_t g_adc_shared_last_err;

#endif /* ADC_SHARED_H_ */
