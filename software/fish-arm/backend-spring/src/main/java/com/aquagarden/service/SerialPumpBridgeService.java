package com.aquagarden.service;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.Map;

@Service
public class SerialPumpBridgeService {
    private final ObjectMapper objectMapper;
    private final HttpClient httpClient;
    private final boolean enabled;
    private final String baseUrl;

    public SerialPumpBridgeService(
            ObjectMapper objectMapper,
            @Value("${aquagarden.serial-pump.enabled:true}") boolean enabled,
            @Value("${aquagarden.serial-pump.base-url:http://127.0.0.1:18080}") String baseUrl) {
        this.objectMapper = objectMapper;
        this.enabled = enabled;
        this.baseUrl = stripTrailingSlash(baseUrl);
        this.httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(2))
                .build();
    }

    public boolean isEnabled() {
        return enabled;
    }

    public AquaBridgeService.BridgeResponse pumpStart(int pwm) throws JsonProcessingException, InterruptedException {
        return post("/api/pump/start", Map.of("pwm", pwm));
    }

    public AquaBridgeService.BridgeResponse pumpPwm(int pwm) throws JsonProcessingException, InterruptedException {
        return post("/api/pump/pwm", Map.of("pwm", pwm));
    }

    public AquaBridgeService.BridgeResponse pumpStop() throws InterruptedException {
        return postNoBody("/api/pump/stop");
    }

    public AquaBridgeService.BridgeResponse pumpAuto() throws InterruptedException {
        return postNoBody("/api/pump/auto");
    }

    public AquaBridgeService.BridgeResponse pumpManual(int on, int pwm) throws JsonProcessingException, InterruptedException {
        return post("/api/pump/manual", Map.of("on", on, "pwm", pwm));
    }

    public AquaBridgeService.BridgeResponse pumpPulse(int seconds, int pwm) throws JsonProcessingException, InterruptedException {
        return post("/api/pump/pulse", Map.of("seconds", seconds, "pwm", pwm));
    }

    public AquaBridgeService.BridgeResponse status() throws InterruptedException {
        return request("GET", "/api/status", null);
    }

    private AquaBridgeService.BridgeResponse postNoBody(String path) throws InterruptedException {
        return request("POST", path, null);
    }

    private AquaBridgeService.BridgeResponse post(String path, Map<String, Object> body)
            throws JsonProcessingException, InterruptedException {
        return request("POST", path, objectMapper.writeValueAsString(body));
    }

    private AquaBridgeService.BridgeResponse request(String method, String path, String jsonBody)
            throws InterruptedException {
        try {
            HttpRequest.Builder builder = HttpRequest.newBuilder()
                    .uri(URI.create(baseUrl + normalizePath(path)))
                    .timeout(Duration.ofSeconds(5));
            if ("POST".equals(method)) {
                HttpRequest.BodyPublisher publisher = jsonBody == null
                        ? HttpRequest.BodyPublishers.noBody()
                        : HttpRequest.BodyPublishers.ofString(jsonBody);
                builder.POST(publisher).header("Content-Type", "application/json");
            } else {
                builder.GET();
            }
            HttpResponse<String> response = httpClient.send(builder.build(), HttpResponse.BodyHandlers.ofString());
            return new AquaBridgeService.BridgeResponse(response.statusCode(), response.body());
        } catch (IOException | IllegalArgumentException e) {
            String detail = e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName();
            try {
                String body = objectMapper.writeValueAsString(Map.of(
                        "ok", false,
                        "message", "无法连接本机串口水泵服务（" + baseUrl + normalizePath(path)
                                + "）：" + detail + "。请确认 serial_bridge.py 正在运行。"));
                return new AquaBridgeService.BridgeResponse(503, body);
            } catch (JsonProcessingException jpe) {
                throw new UncheckedIOException(jpe);
            }
        }
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
}
