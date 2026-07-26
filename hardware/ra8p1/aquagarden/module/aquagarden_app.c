#include "aquagarden_app.h"
#include "sensor_fusion.h"
#include "../device/dev_atomizer_driver.h"
#include "../device/dev_ds18b20_driver.h"
#include "../device/dev_pump_driver.h"
#include "../device/dev_sht30_driver.h"
#include "../device/dev_soil_humid_sen_driver.h"
#include "../device/dev_tds_driver.h"
#include "../device/dev_usb_light_driver.h"
#include "FreeRTOS.h"
#include "task.h"

#define AQUA_DEFAULT_PUMP_PWM    (60U)
#define AQUA_DEFAULT_SOIL_TH     (30U)
#define AQUA_DEFAULT_SOIL_HYS    (10U)
#define AQUA_DEFAULT_TEMP_HIGH_C (30.0F)
#define AQUA_DEFAULT_TDS_LOW_NTU (300U)
#define AQUA_ADC_PERIOD_TICKS    (4U) /* 40 ms */

static aqua_snapshot_t s_snapshot;

static uint8_t  s_pump_manual_mode;
static uint8_t  s_pump_manual_pwm  = AQUA_DEFAULT_PUMP_PWM;
/* 0 until host sends a pump command; stay in brake until then. */
static uint8_t  s_pump_host_armed;
static uint8_t  s_soil_threshold   = AQUA_DEFAULT_SOIL_TH;
static uint8_t  s_soil_hysteresis  = AQUA_DEFAULT_SOIL_HYS;
static uint8_t  s_enable_soil       = 1U;
static uint8_t  s_enable_water_temp = 1U;
static uint8_t  s_enable_tds        = 1U;
static float    s_water_temp_high_c = AQUA_DEFAULT_TEMP_HIGH_C;
static uint16_t s_tds_low_ntu       = AQUA_DEFAULT_TDS_LOW_NTU;

static uint32_t s_comm_error_count;
static uint8_t  s_soil_sensor_fault;
static bool     s_soil_valid;
static bool     s_water_temp_valid;
static bool     s_tds_valid;
static bool     s_water_temp_high;
static bool     s_tds_low;
static uint32_t s_tick_10ms;

static void pump_arm_from_host(void)
{
    s_pump_host_armed = 1U;
}

static uint8_t clamp_u8_percent(uint8_t value)
{
    return (value > 100U) ? 100U : value;
}

static uint32_t app_now_ms(void)
{
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void app_sample_water_temp(void)
{
    float temp;
    fsp_err_t err = dev_ds18b20_measure(&temp);

    if (FSP_SUCCESS == err)
    {
        s_snapshot.water_temp_c = temp;
        s_water_temp_valid = true;
    }
    else if (FSP_ERR_IN_USE == err)
    {
        /* Conversion in progress; keep last value. */
    }
    else if (s_snapshot.uwt_retry_count < UINT16_MAX)
    {
        s_snapshot.uwt_retry_count++;
    }
}

static void app_sample_air(void)
{
    float temp;
    float humi;

    if (dev_sht30_get(&temp, &humi))
    {
        s_snapshot.air_temp_c = temp;
        s_snapshot.air_humi_pct = humi;
    }
    s_snapshot.air_retry_count = dev_sht30_get_fail_count();
}

static void app_sample_adc(void)
{
    uint16_t raw;
    uint8_t  soil_pct;
    float    voltage;
    uint16_t ntu;

    if (FSP_SUCCESS == dev_soil_read(&raw, &soil_pct))
    {
        s_snapshot.soil_moisture_pct = soil_pct;
        s_soil_valid = true;
        s_soil_sensor_fault = 0U;
    }
    else
    {
        s_soil_valid = false;
        s_soil_sensor_fault = 1U;
    }

    if (FSP_SUCCESS == dev_tds_read(&raw, &voltage, &ntu))
    {
        s_snapshot.tds_ntu = ntu;
        s_tds_valid = true;
    }
    else if (s_snapshot.tds_retry_count < UINT16_MAX)
    {
        s_snapshot.tds_retry_count++;
    }
}

static void app_apply_control(void)
{
    fusion_config_t cfg;
    fusion_inputs_t in;
    fusion_outputs_t out;
    uint8_t target;

    cfg.enable_soil       = s_enable_soil;
    cfg.enable_water_temp = s_enable_water_temp;
    cfg.enable_tds        = s_enable_tds;
    cfg.soil_threshold    = s_soil_threshold;
    cfg.soil_hysteresis   = s_soil_hysteresis;
    cfg.water_temp_high_c = s_water_temp_high_c;
    cfg.tds_low_ntu       = s_tds_low_ntu;
    cfg.auto_pump_pwm     = AQUA_DEFAULT_PUMP_PWM;

    in.soil_pct          = s_snapshot.soil_moisture_pct;
    in.soil_valid        = s_soil_valid;
    in.water_temp_c      = s_snapshot.water_temp_c;
    in.water_temp_valid  = s_water_temp_valid;
    in.tds_ntu           = s_snapshot.tds_ntu;
    in.tds_valid         = s_tds_valid;
    in.prev_need_watering = s_snapshot.need_watering;

    sensor_fusion_eval(&cfg, &in, &out);

    s_water_temp_high = out.water_temp_high;
    s_tds_low = out.tds_low;
    s_snapshot.need_watering = out.need_watering;

    if (0U == s_pump_host_armed)
    {
        /* Boot / idle: keep DRV8870 brake until a host pump command arrives. */
        (void) dev_pump_brake();
        s_snapshot.pump_pwm_pct = 0U;
        return;
    }

    target = (0U != s_pump_manual_mode) ? s_pump_manual_pwm : out.auto_pump_pwm;

    (void) dev_pump_set_pwm(target);
    s_snapshot.pump_pwm_pct = dev_pump_get_pwm();
}

static void app_update_alarm_flags(void)
{
    uint32_t alarm = 0U;

    if (0U != s_soil_sensor_fault)
    {
        alarm |= AQUA_ALARM_SOIL_SENSOR_FAULT;
    }
    if (s_snapshot.air_retry_count > 0U)
    {
        alarm |= AQUA_ALARM_AIR_READ_FAIL;
    }
    if (s_snapshot.tds_retry_count > 0U)
    {
        alarm |= AQUA_ALARM_TDS_READ_FAIL;
    }
    if (s_snapshot.uwt_retry_count > 0U)
    {
        alarm |= AQUA_ALARM_UWT_READ_FAIL;
    }
    if (s_water_temp_high)
    {
        alarm |= AQUA_ALARM_WATER_TEMP_HIGH;
    }
    if (s_tds_low)
    {
        alarm |= AQUA_ALARM_TDS_LOW;
    }
    if (s_comm_error_count > 0U)
    {
        alarm |= AQUA_ALARM_COMM_RX_ERROR;
    }
    if (FSP_SUCCESS != dev_usb_light_get_last_error())
    {
        alarm |= AQUA_ALARM_USB_LIGHT_FAULT;
    }

    s_snapshot.alarm_flags = alarm;
}

void aqua_app_init(void)
{
    s_snapshot.usb_light_mode = 0xFFU;
    s_snapshot.atomizer_state = 0U;
    s_snapshot.timestamp_ms = app_now_ms();

    dev_atomizer_init();
    dev_pump_init();
    dev_ds18b20_init();
    dev_sht30_init();
    dev_soil_init();
    dev_tds_init();
    dev_usb_light_init();
}

void aqua_app_process_10ms(void)
{
    s_tick_10ms++;
    s_snapshot.timestamp_ms = app_now_ms();

    dev_usb_light_process();
    dev_sht30_process();

    app_sample_water_temp();
    app_sample_air();

    if ((s_tick_10ms % AQUA_ADC_PERIOD_TICKS) == 0U)
    {
        app_sample_adc();
    }

    app_apply_control();

    s_snapshot.atomizer_state = dev_atomizer_get();
    s_snapshot.usb_light_mode = dev_usb_light_get_mode();
    app_update_alarm_flags();
}

void aqua_app_get_snapshot(aqua_snapshot_t * p_out)
{
    if (NULL != p_out)
    {
        *p_out = s_snapshot;
    }
}

void aqua_app_note_comm_error(void)
{
    if (s_comm_error_count < UINT32_MAX)
    {
        s_comm_error_count++;
    }
}

void aqua_app_set_pump_manual(uint8_t manual_mode, uint8_t pwm_percent)
{
    pump_arm_from_host();
    s_pump_manual_mode = (0U == manual_mode) ? 0U : 1U;
    s_pump_manual_pwm = clamp_u8_percent(pwm_percent);
}

void aqua_app_pump_start(bool has_pwm, uint8_t pwm_percent)
{
    pump_arm_from_host();
    s_pump_manual_mode = 1U;
    if (has_pwm)
    {
        s_pump_manual_pwm = clamp_u8_percent(pwm_percent);
    }
    if (0U == s_pump_manual_pwm)
    {
        s_pump_manual_pwm = AQUA_DEFAULT_PUMP_PWM;
    }
}

void aqua_app_pump_stop(void)
{
    pump_arm_from_host();
    s_pump_manual_mode = 1U;
    s_pump_manual_pwm = 0U;
}

void aqua_app_set_pump_pwm(uint8_t pwm_percent)
{
    pump_arm_from_host();
    s_pump_manual_mode = 1U;
    s_pump_manual_pwm = clamp_u8_percent(pwm_percent);
}

void aqua_app_set_pump_auto(void)
{
    pump_arm_from_host();
    s_pump_manual_mode = 0U;
}

void aqua_app_set_soil_config(uint8_t threshold, uint8_t hysteresis)
{
    s_soil_threshold = clamp_u8_percent(threshold);
    s_soil_hysteresis = clamp_u8_percent(hysteresis);
}

void aqua_app_set_linkage(uint8_t enable_bits, int16_t temp_high_x10, uint16_t tds_low_ntu)
{
    float temp_high = (float) temp_high_x10 / 10.0F;

    if (temp_high < 0.0F)
    {
        temp_high = 0.0F;
    }
    if (temp_high > 100.0F)
    {
        temp_high = 100.0F;
    }

    s_enable_soil = (0U != (enable_bits & 0x01U)) ? 1U : 0U;
    s_enable_water_temp = (0U != (enable_bits & 0x02U)) ? 1U : 0U;
    s_enable_tds = (0U != (enable_bits & 0x04U)) ? 1U : 0U;
    s_water_temp_high_c = temp_high;
    s_tds_low_ntu = tds_low_ntu;
}

fsp_err_t aqua_app_set_usb_light_mode(uint8_t mode)
{
    fsp_err_t err = dev_usb_light_set_mode(mode);
    if (FSP_SUCCESS == err)
    {
        s_snapshot.usb_light_mode = mode;
    }
    return err;
}

fsp_err_t aqua_app_set_atomizer(uint8_t state)
{
    fsp_err_t err;

    if (state > 1U)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    err = dev_atomizer_set(state);
    if (FSP_SUCCESS == err)
    {
        s_snapshot.atomizer_state = dev_atomizer_get();
    }
    return err;
}
