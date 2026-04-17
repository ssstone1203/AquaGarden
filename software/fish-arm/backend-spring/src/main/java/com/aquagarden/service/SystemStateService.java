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

    private static Map<String, Integer> basePosition() {
        Map<String, Integer> m = new HashMap<>();
        m.put("x", 0);
        m.put("y", 0);
        m.put("z", 0);
        return m;
    }

    public synchronized SensorSnapshot readSensorsWithNoise() {
        double temperature = round(25.0 + random.nextDouble() - 0.5, 2);
        double ph = round(7.0 + (random.nextDouble() * 0.4 - 0.2), 2);
        double oxygen = round(8.0 + (random.nextDouble() * 0.6 - 0.3), 2);
        double turbidity = round(10.0 + (random.nextDouble() * 2 - 1), 2);
        return new SensorSnapshot(temperature, ph, oxygen, turbidity);
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
