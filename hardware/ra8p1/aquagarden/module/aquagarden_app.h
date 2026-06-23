#ifndef AQUAGARDEN_APP_H_
#define AQUAGARDEN_APP_H_

#include <stdbool.h>
#include <stdint.h>
#include "bsp_api.h"

typedef enum e_aqua_alarm_flags
{
    AQUA_ALARM_SOIL_SENSOR_FAULT = (1UL << 0),
    AQUA_ALARM_AIR_READ_FAIL     = (1UL << 2),
    AQUA_ALARM_TDS_READ_FAIL     = (1UL << 3),
    AQUA_ALARM_UWT_READ_FAIL     = (1UL << 4),
    AQUA_ALARM_WATER_TEMP_HIGH   = (1UL << 5),
    AQUA_ALARM_TDS_LOW           = (1UL << 6),
    AQUA_ALARM_COMM_RX_ERROR     = (1UL << 8),
    AQUA_ALARM_USB_LIGHT_FAULT   = (1UL << 9),
    AQUA_ALARM_ATOMIZER_FAULT    = (1UL << 10),
} aqua_alarm_flags_t;

typedef struct st_aqua_snapshot
{
    uint32_t timestamp_ms;
    float    air_temp_c;
    float    air_humi_pct;
    float    water_temp_c;
    uint8_t  soil_moisture_pct;
    uint16_t tds_ntu;
    uint8_t  pump_pwm_pct;
    uint8_t  need_watering;
    uint8_t  atomizer_state;
    uint8_t  usb_light_mode;
    uint32_t alarm_flags;
    uint16_t air_retry_count;
    uint16_t tds_retry_count;
    uint16_t uwt_retry_count;
} aqua_snapshot_t;

void aqua_app_init(void);
void aqua_app_process_10ms(void);
void aqua_app_get_snapshot(aqua_snapshot_t * p_out);

void aqua_app_note_comm_error(void);
void aqua_app_set_pump_manual(uint8_t manual_mode, uint8_t pwm_percent);
void aqua_app_pump_start(bool has_pwm, uint8_t pwm_percent);
void aqua_app_pump_stop(void);
void aqua_app_set_pump_pwm(uint8_t pwm_percent);
void aqua_app_set_pump_auto(void);
void aqua_app_set_soil_config(uint8_t threshold, uint8_t hysteresis);
void aqua_app_set_linkage(uint8_t enable_bits, int16_t temp_high_x10, uint16_t tds_low_ntu);
fsp_err_t aqua_app_set_usb_light_mode(uint8_t mode);
fsp_err_t aqua_app_set_atomizer(uint8_t state);

#endif /* AQUAGARDEN_APP_H_ */
