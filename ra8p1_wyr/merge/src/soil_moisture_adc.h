#ifndef SOIL_MOISTURE_ADC_H_
#define SOIL_MOISTURE_ADC_H_

#include <stdint.h>
#include "Sensor_Task.h"

/* RA8P1 ADC_B is 12-bit in this project configuration. */
#define SOIL_ADC_MAX_VALUE         (4095U)

/* P000 / AN000 on CPKHMI-RA8P1 expansion board ADC header pin "000". */
#define SOIL_ADC_CHANNEL           (ADC_CHANNEL_0)

/* Two-point calibration values (tunable in Keil Watch window). */
#define SOIL_ADC_DRY_CAL_RAW       (3200U)
#define SOIL_ADC_WET_CAL_RAW       (1500U)

extern volatile uint16_t g_soil_adc_cal_dry_raw;
extern volatile uint16_t g_soil_adc_cal_wet_raw;

void     soil_adc_init(void);
uint16_t soil_adc_read_raw(void);
uint8_t  soil_adc_raw_to_percent(uint16_t raw_value);

#endif /* SOIL_MOISTURE_ADC_H_ */
