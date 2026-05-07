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

    public AquaBridgeService(
            ObjectMapper objectMapper,
            @Value("${aquagarden.bridge.base-url:http://127.0.0.1:18080}") String baseUrl) {
        this.objectMapper = objectMapper;
        this.baseUrl = stripTrailingSlash(baseUrl);
        this.httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(3))
                .build();
    }

    public Map<String, Object> status() {
        try {
            BridgeResponse response = request("GET", "/api/status", null);
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
        return request("POST", "/api/task/" + task, null);
    }

    public BridgeResponse armHome() throws InterruptedException {
        return request("POST", "/api/arm/home", null);
    }

    public BridgeResponse armPose(Map<String, Object> payload) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(payload);
        return request("POST", "/api/arm/pose", body);
    }

    public BridgeResponse armGripper(Map<String, Object> payload) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(payload);
        return request("POST", "/api/arm/gripper", body);
    }

    public BridgeResponse moveRail(int position) throws IOException, InterruptedException {
        return railPosition(position);
    }

    public BridgeResponse railPosition(int position) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("position", position));
        return request("POST", "/api/rail/position", body);
    }

    /**
     * 水泵短时运行（秒），由树莓派 AquaGarden Bridge 执行；未实现时通常返回 404。
     */
    public BridgeResponse pumpPulse(int seconds) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("seconds", seconds));
        return request("POST", "/api/pump/pulse", body);
    }

    public BridgeResponse pumpStart(int pwm) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("pwm", pwm));
        return request("POST", "/api/pump/start", body);
    }

    public BridgeResponse pumpPwm(int pwm) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("pwm", pwm));
        return request("POST", "/api/pump/pwm", body);
    }

    public BridgeResponse pumpAuto() throws InterruptedException {
        return request("POST", "/api/pump/auto", null);
    }

    /** 关泵：对应 aqua_spi_cli pump stop（不可误用 manual 0 0，固件会进入自动模式）。 */
    public BridgeResponse pumpStop() throws InterruptedException {
        return request("POST", "/api/pump/stop", null);
    }

    public BridgeResponse pumpManual(int on, int pwm) throws JsonProcessingException, InterruptedException {
        String body = objectMapper.writeValueAsString(Map.of("on", on, "pwm", pwm));
        return request("POST", "/api/pump/manual", body);
    }

    public void streamVideo(String mode, OutputStream outputStream) throws IOException {
        String path = "depth".equals(mode) ? "/video/depth.mjpg" : "/video/rgb.mjpg";
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

    public record BridgeResponse(int statusCode, String body) {}
}
