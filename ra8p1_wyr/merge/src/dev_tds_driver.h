#ifndef DEV_TDS_DRIVER_H_
#define DEV_TDS_DRIVER_H_

#include <stdint.h>
#include "Sensor_Task.h"

/* RA8P1 ADC_B is 12-bit in this project configuration. */
#define TDS_ADC_MAX_VALUE          (4095U)
#define TDS_ADC_INVALID_RAW        (0xFFFFU)

/* P001 / AN001 — TDS probe (soil uses AN000 on P000). */
#define TDS_ADC_CHANNEL            (ADC_CHANNEL_1)

/* MCU analog reference equals board VCC (3.3 V). */
#define TDS_ADC_VREF_V             (3.3f)

#define TDS_MEDIAN_FILTER_LEN      (30U)
#define TDS_SAMPLE_INTERVAL_MS     (40U)
#define TDS_UPDATE_INTERVAL_MS     (800U)

void     tds_adc_init(void);
uint16_t tds_adc_read_raw(void);
void     tds_driver_init(void);
void     tds_driver_process(void);

/* Keil Watch window variables. */
extern volatile uint16_t g_tds_adc_raw;
extern volatile uint16_t g_tds_adc_median_raw;
extern volatile float    g_tds_voltage_v;
extern volatile float    g_tds_value_ppm;
extern volatile uint32_t g_tds_sample_count;
extern volatile uint8_t  g_tds_sensor_state;
extern volatile float    g_tds_water_temp_c;

#endif /* DEV_TDS_DRIVER_H_ */
