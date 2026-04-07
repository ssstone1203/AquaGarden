/* SHT30 (SHT3x-DIS) driver: single-shot measurement over I2C (FSP r_iic_b_master API). */

#ifndef SHT30_H
#define SHT30_H

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

/* Keil Watch 窗口可直接观察 */
extern volatile float g_sht30_temperature_c;
extern volatile float g_sht30_temperature_f; /* °F，由摄氏度换算 */
extern volatile float g_sht30_humidity_rh;
/* 手册中的原始 16 位输出（0～65535，换算前；大端 MSB 在前） */
extern volatile uint16_t g_sht30_temperature_raw;
extern volatile uint16_t g_sht30_humidity_raw;
/* 0=idle, 1=write command, 2=read bytes */
extern volatile uint32_t g_sht30_last_i2c_stage;
extern volatile uint32_t g_sht30_last_status; /* last FSP error code (FSP_SUCCESS = 0) */

/**
 * Open I2C master and prepare driver（7 位地址，如 ADDR 接 GND 为 0x44、接 VDD 为 0x45）。
 */
fsp_err_t sht30_init(i2c_master_instance_t const * p_i2c, uint8_t i2c_address_7bit);

/**
 * 运行时修改 7 位地址（ADDR 引脚在硬件上须保持稳定）。
 */
void sht30_set_address_7bit(uint8_t i2c_address_7bit);

/**
 * Single-shot, high repeatability, clock stretching disabled (datasheet command 0x2400).
 * 按 Sensirion 公式换算为 °C、%RH；华氏度 °F = °C×9/5+32。
 * temperature_c / humidity_rh_percent / temperature_f 均可为 NULL；成功时 g_sht30_* 全局量仍会更新。
 */
fsp_err_t sht30_measure_single_shot(float * temperature_c, float * humidity_rh_percent, float * temperature_f,
    bool verify_crc);

#endif
