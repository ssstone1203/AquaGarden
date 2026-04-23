#ifndef PRESSURE_SENSOR_H_
#define PRESSURE_SENSOR_H_

#include "hal_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRESSURE_SENSOR_COUNT    (3U)
#define PRESSURE_SENSOR_MAX_KG   (5.0f)

typedef struct st_pressure_sample
{
    uint16_t adc_raw[PRESSURE_SENSOR_COUNT];
    float    voltage_v[PRESSURE_SENSOR_COUNT];
    float    pressure_kg[PRESSURE_SENSOR_COUNT];
} pressure_sample_t;

fsp_err_t pressure_sensor_init(void);
fsp_err_t pressure_sensor_read(pressure_sample_t * p_sample);

#ifdef __cplusplus
}
#endif

#endif /* PRESSURE_SENSOR_H_ */
