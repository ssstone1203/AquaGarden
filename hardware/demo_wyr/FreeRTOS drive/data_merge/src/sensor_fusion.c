#include "sensor_fusion.h"

#include "FreeRTOS.h"
#include "task.h"

volatile uint16_t g_soil_adc_raw             = 0U;
volatile uint8_t  g_soil_moisture_percent    = 0U;
volatile uint32_t g_soil_sample_count        = 0U;
volatile uint8_t  g_soil_sensor_state        = 0U;
volatile uint16_t g_soil_last_valid_raw      = 0U;
volatile uint8_t  g_soil_need_watering       = 0U;
volatile uint8_t  g_soil_watering_threshold  = 30U;
volatile uint8_t  g_soil_watering_hysteresis = 10U;

volatile pressure_sample_t g_pressure_latest      = {0};
volatile uint32_t          g_pressure_sample_count = 0U;
volatile fsp_err_t         g_pressure_last_err     = FSP_ERR_NOT_INITIALIZED;
volatile fsp_err_t         g_air_last_err          = FSP_ERR_NOT_INITIALIZED;
volatile fsp_err_t         g_wqs_last_err          = FSP_ERR_NOT_INITIALIZED;
volatile uint32_t          g_air_retry_count       = 0U;
volatile uint32_t          g_wqs_retry_count       = 0U;
volatile uint32_t          g_uwt_retry_count       = 0U;

volatile uint8_t g_pump_manual_mode          = 0U;
volatile uint8_t g_pump_manual_power_percent = 60U;
volatile uint8_t g_pump_target_power_percent = 0U;
volatile uint8_t g_pump_actual_power_percent = 0U;
volatile uint8_t g_pump_running              = 0U;
volatile uint8_t g_control_need_watering     = 0U;
volatile uint8_t  g_pump_cycle_enable           = 1U;
volatile uint8_t  g_pump_cycle_start            = 1U;
volatile uint8_t  g_pump_cycle_power_percent    = 40U;
volatile uint32_t g_pump_cycle_run_time_ms      = 3000U;
volatile uint32_t g_pump_cycle_stop_time_ms     = 2000U;
volatile uint32_t g_pump_cycle_interval_time_ms = 1000U;
volatile uint32_t g_pump_cycle_total_count      = 3U;  /* 0 means infinite loop */
volatile uint32_t g_pump_cycle_done_count       = 0U;
volatile uint8_t  g_pump_cycle_active           = 0U;
volatile uint8_t  g_pump_cycle_state            = 0U;  /* 0:idle, 1:run, 2:stop, 3:interval, 4:done */

volatile uint8_t g_ctrl_enable_soil       = 1U;
volatile uint8_t g_ctrl_enable_water_temp = 1U;
volatile uint8_t g_ctrl_enable_wqi        = 1U;
volatile float   g_ctrl_water_temp_high_c = 30.0F;
volatile uint8_t g_ctrl_wqi_low_threshold = 60U;

volatile uint32_t g_alarm_flags             = 0U;
volatile uint32_t g_alarm_latched_flags     = 0U;
volatile uint32_t g_comm_tx_seq             = 0U;
volatile uint8_t  g_comm_last_tx_ok         = 0U;
volatile uint32_t g_comm_rx_cmd_count       = 0U;
volatile uint32_t g_comm_rx_crc_error_count = 0U;

volatile uint32_t g_jscope_time_ms        = 0U;
volatile float    g_jscope_pressure_kg_0  = 0.0F;
volatile float    g_jscope_pressure_kg_1  = 0.0F;
volatile float    g_jscope_pressure_kg_2  = 0.0F;
volatile uint8_t  g_jscope_soil_percent   = 0U;
volatile float    g_jscope_air_temp_c     = 0.0F;
volatile float    g_jscope_air_humidity_rh = 0.0F;
volatile float    g_jscope_water_temp_c   = 0.0F;
volatile float    g_jscope_wqs_wqi        = 0.0F;
volatile float    g_jscope_pump_power_pct = 0.0F;
volatile uint32_t g_jscope_alarm_flags    = 0U;

void sensor_fusion_update_jscope_time(void)
{
    g_jscope_time_ms = (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}
