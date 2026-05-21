package com.aquagarden.web;

import java.time.Instant;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import com.aquagarden.dto.SensorSnapshot;
import com.aquagarden.entity.SensorReading;
import com.aquagarden.repo.SensorReadingRepository;
import com.aquagarden.service.SensorReadingRetentionService;
import com.aquagarden.service.SystemStateService;
import com.aquagarden.websocket.LogWebSocketHandler;

@RestController
public class SensorController {
    private static final Logger log = LoggerFactory.getLogger(SensorController.class);

    private final SystemStateService systemStateService;
    private final LogWebSocketHandler wsHandler;
    private final SensorReadingRepository readingRepo;
    private final SensorReadingRetentionService retentionService;
    private final String deviceUploadToken;
    private final long realtimeMaxAgeMs;

    public SensorController(SystemStateService systemStateService,
                            LogWebSocketHandler wsHandler,
                            SensorReadingRepository readingRepo,
                            SensorReadingRetentionService retentionService,
                            @Value("${aquagarden.device-upload.token:}") String deviceUploadToken,
                            @Value("${aquagarden.sensors.realtime-max-age-ms:10000}") long realtimeMaxAgeMs) {
        this.systemStateService = systemStateService;
        this.wsHandler          = wsHandler;
        this.readingRepo        = readingRepo;
        this.retentionService   = retentionService;
        this.deviceUploadToken  = deviceUploadToken == null ? "" : deviceUploadToken;
        this.realtimeMaxAgeMs   = realtimeMaxAgeMs > 0 ? realtimeMaxAgeMs : 10000L;
    }

    /**
     * 最小可用接口：前端按 sensorId 获取最新单路传感器值。
     * 数据来源：由设备端桥接脚本通过 POST /api/sensors/ingest 上报到后端内存快照。
     * 支持 sensorId:
     *   temp-01          -> water_temp
     *   air-temp-01      -> air_temp
     *   humidity-01      -> air_humidity
     *   wqi-01           -> wqi
     *   soil-moisture-01 -> soil_moisture
     */
    @GetMapping("/api/sensor/latest")
    public Map<String, Object> latest(@RequestParam String sensorId) {
        SensorSnapshot s = systemStateService.readSensorsWithNoise();
        long ts = systemStateService.hasHardwareSnapshot()
                ? systemStateService.latestHardwareTimestamp()
                : System.currentTimeMillis();

        Map<String, Object> data = new HashMap<>();
        data.put("sensorId", sensorId);
        switch (sensorId) {
            case "temp-01" -> {
                data.put("value", s.waterTemp());
                data.put("unit", "C");
            }
            case "air-temp-01" -> {
                data.put("value", s.airTemp());
                data.put("unit", "C");
            }
            case "humidity-01" -> {
                data.put("value", s.airHumidity());
                data.put("unit", "%RH");
            }
            case "wqi-01" -> {
                data.put("value", s.wqi());
                data.put("unit", "index");
            }
            case "soil-moisture-01" -> {
                data.put("value", s.soilMoisture());
                data.put("unit", "%");
            }
            default -> {
                return Map.of(
                        "code", 400,
                        "msg", "unknown sensorId: " + sensorId,
                        "data", Map.of(
                                "sensorId", sensorId,
                                "value", null,
                                "unit", "",
                                "ts", ts
                        )
                );
            }
        }
        data.put("ts", ts);

        Map<String, Object> resp = new HashMap<>();
        resp.put("code", 0);
        resp.put("msg", "ok");
        resp.put("data", data);
        return resp;
    }

    @GetMapping("/api/debug/whoami")
    public Map<String, Object> whoami() {
        return Map.of(
                "service", "spring-boot-aquagarden",
                "port", "8090",
                "ts", System.currentTimeMillis()
        );
    }

    /**
     * 前端轮询接口，返回最新传感器快照（真实或模拟）。
     * 字段与 MCU 上行帧对应：
     *   water_temp   DS18B20 水温 (°C)
     *   air_temp     SHT30 空气温度 (°C)
     *   air_humidity SHT30 空气湿度 (%RH)
     *   wqi          WQM11S 水质综合指数 (0-100)
     *   soil_moisture 土壤湿度 (0-100 %)
     */
    @GetMapping("/api/sensors")
    public Map<String, Object> sensors() {
        boolean fresh = systemStateService.hasFreshHardwareSnapshot(realtimeMaxAgeMs);
        SensorSnapshot s = systemStateService.readFreshSensorsOrDemo(realtimeMaxAgeMs);
        long hardwareTs = systemStateService.latestHardwareTimestamp();
        return Map.of(
                "water_temp",    s.waterTemp(),
                "air_temp",      s.airTemp(),
                "air_humidity",  s.airHumidity(),
                "wqi",           s.wqi(),
                "soil_moisture", s.soilMoisture(),
                "source",        fresh ? "hardware" : "demo",
                "realtime",      fresh,
                "hardwareTs",    hardwareTs,
                "ageMs",         fresh && hardwareTs > 0 ? System.currentTimeMillis() - hardwareTs : -1
        );
    }

    /**
     * 串口桥接脚本调用此接口推送硬件传感器数据。
     * 此接口不需要 JWT，桥接脚本在局域网内调用即可（SecurityConfig 已放行 /api/sensors/ingest）。
     */
    /**
     * 历史传感器数据接口：优先从 SQLite 读取真实记录，无数据时回退模拟值。
     * range: 24h | 7d | 30d
     */
    @GetMapping("/api/sensors/history")
    public List<Map<String, Object>> history(@RequestParam(defaultValue = "24h") String range) {
        long hoursBack = "7d".equals(range) ? 168 : "30d".equals(range) ? 720 : 24;
        Instant since  = Instant.now().minusSeconds(hoursBack * 3600);
        DateTimeFormatter fmt = DateTimeFormatter.ofPattern("yyyy/MM/dd HH:mm").withZone(ZoneId.systemDefault());

        List<SensorReading> rows = readingRepo.findByRecordedAtAfterOrderByRecordedAtAsc(since);
        if (!rows.isEmpty()) {
            List<Map<String, Object>> result = new ArrayList<>();
            for (SensorReading r : rows) {
                result.add(Map.of(
                    "time",          fmt.format(r.getRecordedAt()),
                    "water_temp",    r.getWaterTemp(),
                    "air_temp",      r.getAirTemp(),
                    "air_humidity",  r.getAirHumidity(),
                    "wqi",           r.getWqi(),
                    "soil_moisture", r.getSoilMoisture()
                ));
            }
            return result;
        }

        // 无真实数据时返回模拟历史，避免前端图表空白
        int count = "7d".equals(range) ? 168 : "30d".equals(range) ? 240 : 120;
        long intervalMs = "7d".equals(range) ? 3600_000L : "30d".equals(range) ? 7200_000L : 1800_000L;
        Random rnd = new Random();
        List<Map<String, Object>> result = new ArrayList<>();
        long now = Instant.now().toEpochMilli();
        for (int i = count - 1; i >= 0; i--) {
            long ts = now - (long) i * intervalMs;
            result.add(Map.of(
                "time",          fmt.format(Instant.ofEpochMilli(ts)),
                "water_temp",    round(24.0 + rnd.nextDouble() * 4 - 2, 1),
                "air_temp",      round(26.0 + rnd.nextDouble() * 6 - 3, 1),
                "air_humidity",  round(55.0 + rnd.nextDouble() * 20 - 10, 1),
                "wqi",           round(72.0 + rnd.nextDouble() * 20 - 10, 0),
                "soil_moisture", round(62.0 + rnd.nextDouble() * 20 - 10, 0)
            ));
        }
        return result;
    }

    private static double round(double v, int decimals) {
        double p = Math.pow(10, decimals);
        return Math.round(v * p) / p;
    }

    /**
     * 串口桥接脚本调用此接口推送硬件传感器数据（局域网内，无需 JWT）。
     * 请求体示例：
     * {
     *   "water_temp": 24.5,
     *   "air_temp": 26.3,
     *   "air_humidity": 58.0,
     *   "wqi": 76.0,
     *   "soil_moisture": 63.0
     * }
     */
    @PostMapping("/api/sensors/ingest")
    public ResponseEntity<Void> ingest(@RequestBody Map<String, Double> body) {
        double waterTemp    = body.getOrDefault("water_temp",    24.0);
        double airTemp      = body.getOrDefault("air_temp",      26.0);
        double airHumidity  = body.getOrDefault("air_humidity",  55.0);
        double wqi          = body.getOrDefault("wqi",           70.0);
        double soilMoisture = body.getOrDefault("soil_moisture", 62.0);

        SensorSnapshot snap = new SensorSnapshot(waterTemp, airTemp, airHumidity, wqi, soilMoisture);

        // 更新内存缓存供 GET /api/sensors 使用
        systemStateService.updateFromHardware(snap);

        // WebSocket 实时推送给所有在线前端
        wsHandler.broadcastSensorData(snap);

        // 持久化到 SQLite，供历史图表查询
        readingRepo.save(new SensorReading(
            Instant.now(), waterTemp, airTemp, airHumidity, wqi, soilMoisture
        ));
        retentionService.enforceMaxRecordWindow();

        return ResponseEntity.ok().build();
    }

    /**
     * 设备端上报单路传感器数据接口（局域网 + 设备令牌鉴权）。
     * Header:
     *   X-Device-Token: <token>
     * Body:
     * {
     *   "deviceId": "phytium-01",
     *   "sensorId": "temp-01",
     *   "value": 26.37,
     *   "unit": "C",
     *   "ts": 1715082000000
     * }
     */
    @PostMapping("/api/sensor/upload")
    public ResponseEntity<Map<String, Object>> upload(
            @RequestHeader(value = "X-Device-Token", required = false) String token,
            @RequestBody Map<String, Object> body) {
        log.info("upload token recv='{}', expected='{}'",
                maskToken(token), maskToken(deviceUploadToken));
        if (deviceUploadToken.isBlank()) {
            return ResponseEntity.status(HttpStatus.SERVICE_UNAVAILABLE).body(Map.of(
                    "code", 503,
                    "msg", "device upload token is not configured",
                    "data", Map.of()
            ));
        }
        if (token == null || !deviceUploadToken.equals(token)) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(Map.of(
                    "code", 401,
                    "msg", "invalid device token",
                    "data", Map.of()
            ));
        }

        String sensorId = String.valueOf(body.getOrDefault("sensorId", ""));
        if (sensorId.isBlank()) {
            return ResponseEntity.badRequest().body(Map.of(
                    "code", 400,
                    "msg", "sensorId is required",
                    "data", Map.of()
            ));
        }

        Object valueObj = body.get("value");
        if (!(valueObj instanceof Number)) {
            return ResponseEntity.badRequest().body(Map.of(
                    "code", 400,
                    "msg", "value must be numeric",
                    "data", Map.of()
            ));
        }
        double value = ((Number) valueObj).doubleValue();
        long ts = parseLongOrDefault(body.get("ts"), System.currentTimeMillis());

        SensorSnapshot current = systemStateService.readSensorsWithNoise();
        double waterTemp = current.waterTemp();
        double airTemp = current.airTemp();
        double airHumidity = current.airHumidity();
        double wqi = current.wqi();
        double soilMoisture = current.soilMoisture();

        switch (sensorId) {
            case "temp-01" -> waterTemp = value;
            case "air-temp-01" -> airTemp = value;
            case "humidity-01" -> airHumidity = value;
            case "wqi-01" -> wqi = value;
            case "soil-moisture-01" -> soilMoisture = value;
            default -> {
                return ResponseEntity.badRequest().body(Map.of(
                        "code", 400,
                        "msg", "unknown sensorId: " + sensorId,
                        "data", Map.of("sensorId", sensorId)
                ));
            }
        }

        SensorSnapshot snap = new SensorSnapshot(waterTemp, airTemp, airHumidity, wqi, soilMoisture);
        systemStateService.updateFromHardware(snap, ts);
        wsHandler.broadcastSensorData(snap);
        readingRepo.save(new SensorReading(
                Instant.ofEpochMilli(ts), waterTemp, airTemp, airHumidity, wqi, soilMoisture
        ));
        retentionService.enforceMaxRecordWindow();

        return ResponseEntity.ok(Map.of(
                "code", 0,
                "msg", "ok",
                "data", Map.of(
                        "deviceId", String.valueOf(body.getOrDefault("deviceId", "")),
                        "sensorId", sensorId,
                        "value", value,
                        "ts", ts
                )
        ));
    }

    private static String maskToken(String token) {
        if (token == null) {
            return "<null>";
        }
        if (token.isBlank()) {
            return "<blank>";
        }
        int n = token.length();
        if (n <= 4) {
            return "***";
        }
        return token.substring(0, 2) + "***" + token.substring(n - 2);
    }

    private static long parseLongOrDefault(Object value, long defaultValue) {
        if (value instanceof Number n) {
            return n.longValue();
        }
        if (value instanceof String s) {
            try {
                return Long.parseLong(s);
            } catch (NumberFormatException ignored) {
                return defaultValue;
            }
        }
        return defaultValue;
    }
}
