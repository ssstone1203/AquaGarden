/* SHT30 (SHT3x-DIS) driver: single-shot measurement over I2C (FSP r_iic_master API). */

#ifndef SHT30_H
#define SHT30_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_api.h"
#include "r_i2c_master_api.h"

/*
 * 量程说明（Sensirion SHT3x-DIS 数据手册：规格保证范围 / specified range）
 *   相对湿度：0～100 %RH
 *   温度：−40～125 °C
 * 16 位输出按手册换算式得到的数值范围约为：温度 −45～130 °C，湿度 0～100 %RH（略宽于上表）。
 * 推荐长期工作区间（手册 1.1 Recommended Operating Condition）：5～60 °C、20～80 %RH。
 */
#define SHT30_RH_PCT_SPEC_MIN           (0.0F)
#define SHT30_RH_PCT_SPEC_MAX           (100.0F)
#define SHT30_TEMP_C_SPEC_MIN           (-40.0F)
#define SHT30_TEMP_C_SPEC_MAX           (125.0F)
#define SHT30_TEMP_C_FORMULA_MIN        (-45.0F)
#define SHT30_TEMP_C_FORMULA_MAX        (130.0F)
#define SHT30_TEMP_C_RECOMMENDED_MIN    (5.0F)
#define SHT30_TEMP_C_RECOMMENDED_MAX    (60.0F)
#define SHT30_RH_PCT_RECOMMENDED_MIN    (20.0F)
#define SHT30_RH_PCT_RECOMMENDED_MAX    (80.0F)

/* ADDR 接 VSS → 0x44；接 VDD → 0x45（须与硬件一致） */
#ifndef SHT30_I2C_ADDR_7BIT_DEFAULT
#define SHT30_I2C_ADDR_7BIT_DEFAULT (0x44U)
#endif

/* Keil Watch 窗口可直接观察 */
extern volatile float g_sht30_temperature_c;
extern volatile float g_sht30_temperature_f;
extern volatile float g_sht30_humidity_rh;
extern volatile uint16_t g_sht30_temperature_raw;
extern volatile uint16_t g_sht30_humidity_raw;
extern volatile uint32_t g_sht30_last_i2c_stage;
extern volatile uint32_t g_sht30_last_status;

fsp_err_t sht30_init(i2c_master_instance_t const * p_i2c, uint8_t i2c_address_7bit);
void sht30_set_address_7bit(uint8_t i2c_address_7bit);
fsp_err_t sht30_measure_single_shot(float * temperature_c, float * humidity_rh_percent, float * temperature_f,
    bool verify_crc);

#endif
