#ifndef SENSOR_FUSION_H_
#define SENSOR_FUSION_H_

#include <stdint.h>
#include "bsp_api.h"
#include "pressure_sensor.h"

extern volatile uint16_t g_soil_adc_raw;
extern volatile uint8_t  g_soil_moisture_percent;
extern volatile uint32_t g_soil_sample_count;
extern volatile uint8_t  g_soil_sensor_state;
extern volatile uint16_t g_soil_last_valid_raw;
extern volatile uint8_t  g_soil_need_watering;
extern volatile uint8_t  g_soil_watering_threshold;
extern volatile uint8_t  g_soil_watering_hysteresis;

extern volatile pressure_sample_t g_pressure_latest;
extern volatile uint32_t          g_pressure_sample_count;
extern volatile fsp_err_t         g_pressure_last_err;
extern volatile fsp_err_t         g_air_last_err;
extern volatile fsp_err_t         g_wqs_last_err;
extern volatile uint32_t          g_air_retry_count;
extern volatile uint32_t          g_wqs_retry_count;
extern volatile uint32_t          g_uwt_retry_count;

extern volatile uint8_t g_pump_manual_mode;
extern volatile uint8_t g_pump_manual_power_percent;
extern volatile uint8_t g_pump_target_power_percent;
extern volatile uint8_t g_pump_actual_power_percent;
extern volatile uint8_t g_pump_running;
extern volatile uint8_t g_control_need_watering;

extern volatile uint8_t g_ctrl_enable_soil;
extern volatile uint8_t g_ctrl_enable_water_temp;
extern volatile uint8_t g_ctrl_enable_wqi;
extern volatile float   g_ctrl_water_temp_high_c;
extern volatile uint8_t g_ctrl_wqi_low_threshold;

enum
{
    SENSOR_ALARM_SOIL_SENSOR_FAULT = (1UL << 0),
    SENSOR_ALARM_PRESSURE_READ_FAIL = (1UL << 1),
    SENSOR_ALARM_AIR_READ_FAIL = (1UL << 2),
    SENSOR_ALARM_WQS_READ_FAIL = (1UL << 3),
    SENSOR_ALARM_UWT_READ_FAIL = (1UL << 4),
    SENSOR_ALARM_WATER_TEMP_HIGH = (1UL << 5),
    SENSOR_ALARM_WQI_LOW = (1UL << 6),
    SENSOR_ALARM_PRESSURE_HIGH = (1UL << 7),
    SENSOR_ALARM_COMM_RX_ERROR = (1UL << 8),
};
extern volatile uint32_t g_alarm_flags;
extern volatile uint32_t g_alarm_latched_flags;

extern volatile uint32_t g_comm_tx_seq;
extern volatile uint8_t  g_comm_last_tx_ok;
extern volatile uint32_t g_comm_rx_cmd_count;
extern volatile uint32_t g_comm_rx_crc_error_count;

extern volatile uint32_t g_jscope_time_ms;
extern volatile float    g_jscope_pressure_kg_0;
extern volatile float    g_jscope_pressure_kg_1;
extern volatile float    g_jscope_pressure_kg_2;
extern volatile uint8_t  g_jscope_soil_percent;
extern volatile float    g_jscope_air_temp_c;
extern volatile float    g_jscope_air_humidity_rh;
extern volatile float    g_jscope_water_temp_c;
extern volatile float    g_jscope_wqs_wqi;
extern volatile float    g_jscope_pump_power_pct;
extern volatile uint32_t g_jscope_alarm_flags;

void sensor_fusion_update_jscope_time(void);

#endif /* SENSOR_FUSION_H_ */
