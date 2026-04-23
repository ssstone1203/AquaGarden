package com.aquagarden.dto;

/**
 * 硬件传感器快照（字段与 MCU Communicate_Task_entry.c 上行帧一一对应）：
 *   waterTemp    ← g_uwt_temperature_c / 10.0   DS18B20 水温 (°C)
 *   airTemp      ← g_sht30_temperature_c / 10.0  SHT30 空气温度 (°C)
 *   airHumidity  ← g_sht30_humidity_rh / 10.0    SHT30 空气湿度 (%RH)
 *   wqi          ← wqs_info_wqi                  WQM11S 水质综合指数 (0-100)
 *   soilMoisture ← g_soil_moisture_percent        土壤湿度 ADC (0-100 %)
 */
public record SensorSnapshot(double waterTemp, double airTemp, double airHumidity, double wqi, double soilMoisture) {}
