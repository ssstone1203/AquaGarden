package com.aquagarden.service;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.io.InputStream;
import java.io.UncheckedIOException;
import java.io.OutputStream;
import java.net.URI;
import java.net.URL;
import java.net.URLConnection;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.HashMap;
import java.util.Map;

@Service
public class AquaBridgeService {

    private static final TypeReference<Map<String, Object>> MAP_TYPE = new TypeReference<>() {};
    private final HttpClient httpClient;
    private final ObjectMapper objectMapper;
    private final String baseUrl;
    private final String statusPath;
    private final String taskPathTemplate;
    private final String armHomePath;
    private final String armPosePath;
    private final String armGripperPath;
    private final String railPositionPath;
    private final String pumpPulsePath;
    private final String pumpStartPath;
    private final String pumpPwmPath;
    private final String pumpAutoPath;
    private final String pumpStopPath;
    private final String pumpManualPath;
    private final String rgbVideoPath;
    private final String depthVideoPath;

    public AquaBridgeService(
            ObjectMapper objectMapper,
            @Value("${aquagarden.bridge.base-url:http://127.0.0.1:18080}") String baseUrl,
            @Value("${aquagarden.bridge.paths.status:/api/status}") String statusPath,
            @Value("${aquagarden.bridge.paths.task:/api/task/{task}}") String taskPathTemplate,
            @Value("${aquagarden.bridge.paths.arm-home:/api/arm/home}") String armHomePath,
            @Value("${aquagarden.bridge.paths.arm-pose:/api/arm/pose}") String armPosePath,
            @Value("${aquagarden.bridge.paths.arm-gripper:/api/arm/gripper}") String armGripperPath,
            @Value("${aquagarden.bridge.paths.rail-position:/api/rail/position}") String railPositionPath,
            @Value("${aquagarden.bridge.paths.pump-pulse:/api/pump/pulse}") String pumpPulsePath,
            @Value("${aquagarden.bridge.paths.pump-start:/api/pump/start}") String pumpStartPath,
            @Value("${aquagarden.bridge.paths.pump-pwm:/api/pump/pwm}") String pumpPwmPath,
            @Value("${aquagarden.bridge.paths.pump-auto:/api/pump/auto}") String pumpAutoPath,
            @Value("${aquagarden.bridge.paths.pump-stop:/api/pump/stop}") String pumpStopPath,
            @Value("${aquagarden.bridge.paths.pump-manual:/api/pump/manual}") String pumpManualPath,
            @Value("${aquagarden.bridge.paths.video-rgb:/video/rgb.mjpg}") String rgbVideoPath,
            @Value("${aquagarden.bridge.paths.video-depth:/video/depth.mjpg}") String depthVideoPath) {
        this.objectMapper = objectMapper;
        this.baseUrl = stripTrailingSlash(baseUrl);
        this.statusPath = normalizePath(statusPath);
        this.taskPathTemplate = normalizePath(taskPathTemplate);
        this.armHomePath = normalizePath(armHomePath);
        this.armPosePath = normalizePath(armPosePath);
        this.armGripperPath = normalizePath(armGripperPath);
        this.railPositionPath = normalizePath(railPositionPath);
        this.pumpPulsePath = normalizePath(pumpPulsePath);
        this.pumpStartPath = normalizePath(pumpStartPath);
        this.pumpPwmPath = normalizePath(pumpPwmPath);
        this.pumpAutoPath = normalizePath(pumpAutoPath);
        this.pumpStopPath = normalizePath(pumpStopPath);
        this.pumpManualPath = normalizePath(pumpManualPath);
        this.rgbVideoPath = normalizePath(rgbVideoPath);
        this.depthVideoPath = normalizePath(depthVideoPath);
        this.httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(3))
                .build();
    }

    public Map<String, Object> status() {
        try {
            BridgeResponse response = request("GET", statusPath, null);
            if (response.statusCode() >= 200 && response.statusCode() < 300) {
                return parseMap(response.body());
            }
            if (response.statusCode() == 503) {
                try {
                    Map<String, Object> m = parseMap(response.body());
                    Object msg = m.get("message");
                    if (msg != null) {
                        return offlineStatus(String.valueOf(msg));
                    }
                } catch (IOException ignored) {
                }
            }
            return offlineStatus("Bridge status HTTP " + response.statusCode());
        } catch (InterruptedException | IllegalArgumentException e) {
            if (e instanceof InterruptedException) {
                Thread.currentThread().interrupt();
            }
            return offlineStatus(e.getMessage());
        } catch (IOException e) {
            return offlineStatus(e.getMessage());
        }
    }

    public BridgeResponse task(String task) throws InterruptedException {
        return request("POST", taskPath(task), null);
    }

    public BridgeResponse armHome() throws InterruptedException {
        return request("POST", armHomePath, null);
    }

    public BridgeResponse armPose(Map<String, Object> payload) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(payload);
        return request("POST", armPosePath, body);
    }

    public BridgeResponse armGripper(Map<String, Object> payload) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(payload);
        return request("POST", armGripperPath, body);
    }

    public BridgeResponse moveRail(int position) throws IOException, InterruptedException {
        return railPosition(position);
    }

    public BridgeResponse railPosition(int position) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("position", position));
        return request("POST", railPositionPath, body);
    }

    /**
     * 水泵短时运行（秒），由树莓派 AquaGarden Bridge 执行；未实现时通常返回 404。
     */
    public BridgeResponse pumpPulse(int seconds) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("seconds", seconds));
        return request("POST", pumpPulsePath, body);
    }

    public BridgeResponse pumpStart(int pwm) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("pwm", pwm));
        return request("POST", pumpStartPath, body);
    }

    public BridgeResponse pumpPwm(int pwm) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("pwm", pwm));
        return request("POST", pumpPwmPath, body);
    }

    public BridgeResponse pumpAuto() throws InterruptedException {
        return request("POST", pumpAutoPath, null);
    }

    /** 关泵：由树莓派 Bridge 执行。 */
    public BridgeResponse pumpStop() throws InterruptedException {
        return request("POST", pumpStopPath, null);
    }

    public BridgeResponse pumpManual(int on, int pwm) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("on", on, "pwm", pwm));
        return request("POST", pumpManualPath, body);
    }

    public void streamVideo(String mode, OutputStream outputStream) throws IOException {
        String path = "depth".equals(mode) ? depthVideoPath : rgbVideoPath;
        URLConnection connection = new URL(baseUrl + path).openConnection();
        connection.setConnectTimeout(3000);
        connection.setReadTimeout(0);
        try (InputStream inputStream = connection.getInputStream()) {
            inputStream.transferTo(outputStream);
        }
    }

    public Map<String, Object> parseMap(String body) throws IOException {
        if (body == null || body.isBlank()) {
            return Map.of();
        }
        return objectMapper.readValue(body, MAP_TYPE);
    }

    private BridgeResponse request(String method, String path, String jsonBody) throws InterruptedException {
        try {
            HttpRequest.Builder builder = HttpRequest.newBuilder()
                    .uri(URI.create(baseUrl + path))
                    .timeout(Duration.ofSeconds(8));

            if ("POST".equals(method)) {
                HttpRequest.BodyPublisher publisher = jsonBody == null
                        ? HttpRequest.BodyPublishers.noBody()
                        : HttpRequest.BodyPublishers.ofString(jsonBody);
                builder.POST(publisher).header("Content-Type", "application/json");
            } else {
                builder.GET();
            }

            HttpResponse<String> response = httpClient.send(builder.build(), HttpResponse.BodyHandlers.ofString());
            return new BridgeResponse(response.statusCode(), response.body());
        } catch (IOException e) {
            String detail = e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName();
            String json;
            try {
                json = objectMapper.writeValueAsString(Map.of(
                        "ok", false,
                        "message",
                        "无法连接 AquaGarden Bridge（" + baseUrl + path + "）：" + detail
                                + "。请确认设备上已启动 aqua_bridge 且防火墙放行端口。"));
            } catch (JsonProcessingException jpe) {
                throw new UncheckedIOException(jpe);
            }
            return new BridgeResponse(503, json);
        }
    }

    private static Map<String, Object> offlineStatus(String reason) {
        Map<String, Object> status = new HashMap<>();
        status.put("ok", false);
        status.put("connected", false);
        status.put("busy", false);
        status.put("currentTask", "idle");
        status.put("phase", "offline");
        status.put("railPosition", null);
        status.put("lastError", reason == null ? "Bridge unavailable" : reason);
        status.put("camera", Map.of("hasRgb", false, "hasDepth", false));
        return status;
    }

    private static String stripTrailingSlash(String value) {
        if (value == null || value.isBlank()) {
            return "http://127.0.0.1:18080";
        }
        return value.endsWith("/") ? value.substring(0, value.length() - 1) : value;
    }

    private static String normalizePath(String value) {
        if (value == null || value.isBlank()) {
            return "/";
        }
        return value.startsWith("/") ? value : "/" + value;
    }

    private String taskPath(String task) {
        if (taskPathTemplate.contains("{task}")) {
            return taskPathTemplate.replace("{task}", task);
        }
        return taskPathTemplate.endsWith("/") ? taskPathTemplate + task : taskPathTemplate + "/" + task;
    }

    public record BridgeResponse(int statusCode, String body) {}
}
