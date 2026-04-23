package com.aquagarden.service;

import com.aquagarden.dto.SensorSnapshot;
import org.springframework.stereotype.Service;

import java.time.Instant;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicReference;

@Service
public class SystemStateService {

    private final AtomicReference<String> mode = new AtomicReference<>("demo");
    private final AtomicReference<Map<String, Integer>> robotPosition = new AtomicReference<>(basePosition());
    private final Random random = new Random();

    /** 最近一次由硬件桥接脚本推送的真实传感器快照，null 表示尚未收到真实数据 */
    private final AtomicReference<SensorSnapshot> latestReal = new AtomicReference<>(null);

    private static Map<String, Integer> basePosition() {
        Map<String, Integer> m = new HashMap<>();
        m.put("x", 0);
        m.put("y", 0);
        m.put("z", 0);
        return m;
    }

    /**
     * 接收来自串口桥接脚本的真实传感器数据。
     * 字段与 MCU Communicate_Task_entry.c 上行帧一一对应：
     *   waterTemp    ← g_uwt_temperature_c / 10.0  DS18B20 水温
     *   airTemp      ← g_sht30_temperature_c / 10.0  SHT30 空气温度
     *   airHumidity  ← g_sht30_humidity_rh / 10.0   SHT30 空气湿度
     *   wqi          ← wqs_info_wqi                  WQM11S 水质综合指数 0-100
     *   soilMoisture ← g_soil_moisture_percent        土壤湿度 0-100%
     */
    public void updateFromHardware(SensorSnapshot snapshot) {
        latestReal.set(snapshot);
    }

    /** 有真实数据时返回真实值，否则返回带随机扰动的模拟值（演示 / 未接硬件时使用）。 */
    public SensorSnapshot readSensorsWithNoise() {
        SensorSnapshot real = latestReal.get();
        if (real != null) {
            return real;
        }
        // 模拟数据范围与硬件传感器实际量程匹配：
        // DS18B20 水温：推荐范围 18-32°C；SHT30 空气温度：15-35°C；空气湿度：30-80%RH
        // WQM11S WQI：良好水质通常 60-90；土壤湿度 ADC：40-80%
        double waterTemp   = round(24.0 + random.nextDouble() * 2 - 1, 1);
        double airTemp     = round(26.0 + random.nextDouble() * 4 - 2, 1);
        double airHumidity = round(55.0 + random.nextDouble() * 10 - 5, 1);
        double wqi         = round(72.0 + random.nextDouble() * 12 - 6, 0);
        double soilMoisture = round(62.0 + random.nextDouble() * 10 - 5, 1);
        return new SensorSnapshot(waterTemp, airTemp, airHumidity, wqi, soilMoisture);
    }

    private static double round(double v, int decimals) {
        double p = Math.pow(10, decimals);
        return Math.round(v * p) / p;
    }

    public String getMode() {
        return mode.get();
    }

    public void setMode(String m) {
        mode.set(m);
    }

    public Map<String, Integer> getRobotPosition() {
        return new HashMap<>(robotPosition.get());
    }

    public Map<String, Integer> moveRobot(String direction) {
        Map<String, Integer> pos = new HashMap<>(robotPosition.get());
        int dx = 0, dy = 0, dz = 0;
        switch (direction) {
            case "up" -> dz = 1;
            case "down" -> dz = -1;
            case "left" -> dx = -1;
            case "right" -> dx = 1;
            case "forward" -> dy = 1;
            case "backward" -> dy = -1;
            default -> {
                return pos;
            }
        }
        pos.put("x", pos.get("x") + dx);
        pos.put("y", pos.get("y") + dy);
        pos.put("z", pos.get("z") + dz);
        robotPosition.set(pos);
        return new HashMap<>(pos);
    }

    /** 模拟舵机角度（6路，0-180°），每次调用产生微小随机抖动以体现"运行中"状态。 */
    public int[] getServoAngles() {
        int[] base = {90, 45, 120, 60, 90, 30};
        int[] result = new int[base.length];
        for (int i = 0; i < base.length; i++) {
            result[i] = Math.max(0, Math.min(180, base[i] + (int)(random.nextDouble() * 4 - 2)));
        }
        return result;
    }

    public Instant now() {
        return Instant.now();
    }

    public String randomStatusMessage() {
        String[] messages = {
                "系统运行正常",
                "传感器数据稳定",
                "水质参数正常",
                "自动监控进行中"
        };
        return messages[ThreadLocalRandom.current().nextInt(messages.length)];
    }
}
