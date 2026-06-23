#ifndef SENSOR_FUSION_H_
#define SENSOR_FUSION_H_

#include <stdbool.h>
#include <stdint.h>

/* Linkage configuration (no pressure channel on RA8P1). */
typedef struct st_fusion_config
{
    uint8_t  enable_soil;        /* bit0 */
    uint8_t  enable_water_temp;  /* bit1 */
    uint8_t  enable_tds;         /* bit2 */
    uint8_t  soil_threshold;     /* % : water when soil <= threshold */
    uint8_t  soil_hysteresis;    /* % : stop when soil >= threshold + hysteresis */
    float    water_temp_high_c;  /* C : high water temperature trigger */
    uint16_t tds_low_ntu;        /* NTU : low turbidity/TDS trigger */
    uint8_t  auto_pump_pwm;      /* PWM % applied when auto logic requests watering */
} fusion_config_t;

typedef struct st_fusion_inputs
{
    uint8_t  soil_pct;
    bool     soil_valid;
    float    water_temp_c;
    bool     water_temp_valid;
    uint16_t tds_ntu;
    bool     tds_valid;
    uint8_t  prev_need_watering; /* previous decision, for soil hysteresis latch */
} fusion_inputs_t;

typedef struct st_fusion_outputs
{
    uint8_t need_watering;   /* 0/1 linkage decision */
    uint8_t auto_pump_pwm;   /* suggested PWM in auto mode */
    bool    water_temp_high; /* alarm condition */
    bool    tds_low;         /* alarm condition */
} fusion_outputs_t;

/* Pure function: evaluate linkage from sensor readings and configuration. */
void sensor_fusion_eval(const fusion_config_t * p_cfg,
                        const fusion_inputs_t * p_in,
                        fusion_outputs_t * p_out);

#endif /* SENSOR_FUSION_H_ */
