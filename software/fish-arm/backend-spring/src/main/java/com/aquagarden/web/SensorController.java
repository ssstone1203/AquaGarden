package com.aquagarden.web;

import com.aquagarden.dto.SensorSnapshot;
import com.aquagarden.service.SystemStateService;
import com.aquagarden.websocket.LogWebSocketHandler;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.time.Instant;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Random;

@RestController
public class SensorController {

    private final SystemStateService systemStateService;
    private final LogWebSocketHandler wsHandler;

    public SensorController(SystemStateService systemStateService, LogWebSocketHandler wsHandler) {
        this.systemStateService = systemStateService;
        this.wsHandler = wsHandler;
    }

    /** 前端轮询接口，返回最新传感器快照（真实或模拟）。 */
    @GetMapping("/api/sensors")
    public Map<String, Object> sensors() {
        SensorSnapshot s = systemStateService.readSensorsWithNoise();
        return Map.of(
                "temperature", s.temperature(),
                "ph", s.ph(),
                "oxygen", s.oxygen(),
                "turbidity", s.turbidity(),
                "soil_moisture", s.soilMoisture()
        );
    }

    /**
     * 串口桥接脚本调用此接口推送硬件传感器数据。
     * 请求体示例：
     * {
     *   "temperature": 25.3,
     *   "ph": 7.1,
     *   "oxygen": 8.2,
     *   "turbidity": 1.8,
     *   "soil_moisture": 62.0
     * }
     * 此接口不需要 JWT，桥接脚本在局域网内调用即可（SecurityConfig 已放行 /api/sensors/ingest）。
     */
    /**
     * 历史传感器数据接口（当前生成模拟数据，接入真实 DB 后替换实现即可）。
     * range: 24h | 7d | 30d
     */
    @GetMapping("/api/sensors/history")
    public List<Map<String, Object>> history(@RequestParam(defaultValue = "24h") String range) {
        int count = "7d".equals(range) ? 168 : "30d".equals(range) ? 240 : 120;
        long intervalMs = "7d".equals(range) ? 3600_000L : "30d".equals(range) ? 7200_000L : 1800_000L;
        Random rnd = new Random();
        DateTimeFormatter fmt = DateTimeFormatter.ofPattern("yyyy/MM/dd HH:mm").withZone(ZoneId.systemDefault());
        List<Map<String, Object>> result = new ArrayList<>();
        long now = Instant.now().toEpochMilli();
        for (int i = 0; i < count; i++) {
            long ts = now - (long) i * intervalMs;
            result.add(Map.of(
                "time",         fmt.format(Instant.ofEpochMilli(ts)),
                "temperature",  round(25.0 + rnd.nextDouble() * 4 - 2, 1),
                "ph",           round(7.0 + rnd.nextDouble() * 0.8 - 0.4, 2),
                "oxygen",       round(8.0 + rnd.nextDouble() * 2 - 1, 1),
                "turbidity",    round(12.0 + rnd.nextDouble() * 8, 1),
                "soil_moisture",round(65.0 + rnd.nextDouble() * 14 - 7, 0)
            ));
        }
        return result;
    }

    private static double round(double v, int decimals) {
        double p = Math.pow(10, decimals);
        return Math.round(v * p) / p;
    }

    @PostMapping("/api/sensors/ingest")
    public ResponseEntity<Void> ingest(@RequestBody Map<String, Double> body) {
        double temperature  = body.getOrDefault("temperature",  25.0);
        double ph           = body.getOrDefault("ph",           7.0);
        double oxygen       = body.getOrDefault("oxygen",       8.0);
        double turbidity    = body.getOrDefault("turbidity",    1.5);
        double soilMoisture = body.getOrDefault("soil_moisture", 65.0);
        SensorSnapshot snap = new SensorSnapshot(temperature, ph, oxygen, turbidity, soilMoisture);
        systemStateService.updateFromHardware(snap);
        wsHandler.broadcastSensorData(snap);
        return ResponseEntity.ok().build();
    }
}
