package com.aquagarden.web;

import com.aquagarden.dto.SensorSnapshot;
import com.aquagarden.service.SystemStateService;
import com.aquagarden.websocket.LogWebSocketHandler;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

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
