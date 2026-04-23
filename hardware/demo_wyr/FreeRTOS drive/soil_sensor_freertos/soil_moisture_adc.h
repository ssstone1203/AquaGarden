#ifndef SOIL_MOISTURE_ADC_H_
#define SOIL_MOISTURE_ADC_H_

#include <stdint.h>
#include "hal_data.h"

/* RA6E2 ADC is 12-bit by default. */
#define SOIL_ADC_MAX_VALUE         (4095U)

/* Configure according to your analog pin (AN000..AN028). */
/* SHS_AD net maps to ADC0_AN007 in your RASC/pin configuration. */
#define SOIL_ADC_CHANNEL_INDEX     (7U)

/* Two-point calibration values:
 * dry_raw: probe in dry air/soil
 * wet_raw: probe in water or saturated soil
 */
#define SOIL_ADC_DRY_CAL_RAW       (3200U)
#define SOIL_ADC_WET_CAL_RAW       (1500U)

/* Runtime-tunable calibration points (editable in Watch window). */
extern volatile uint16_t g_soil_adc_cal_dry_raw;
extern volatile uint16_t g_soil_adc_cal_wet_raw;

void     soil_adc_init(void);
uint16_t soil_adc_read_raw(void);
uint8_t  soil_adc_raw_to_percent(uint16_t raw_value);

#endif /* SOIL_MOISTURE_ADC_H_ */
