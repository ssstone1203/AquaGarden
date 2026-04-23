package com.aquagarden.websocket;

import com.aquagarden.dto.SensorSnapshot;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.socket.CloseStatus;
import org.springframework.web.socket.TextMessage;
import org.springframework.web.socket.WebSocketSession;
import org.springframework.web.socket.handler.TextWebSocketHandler;

import java.io.IOException;
import java.time.Instant;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CopyOnWriteArrayList;

@Component
public class LogWebSocketHandler extends TextWebSocketHandler {

    private static final Logger log = LoggerFactory.getLogger(LogWebSocketHandler.class);

    private final CopyOnWriteArrayList<WebSocketSession> sessions = new CopyOnWriteArrayList<>();
    private final ObjectMapper objectMapper = new ObjectMapper();

    @Override
    public void afterConnectionEstablished(WebSocketSession session) {
        sessions.add(session);
        log.info("WebSocket连接建立，当前连接数: {}", sessions.size());
        Map<String, Object> welcome = new HashMap<>();
        welcome.put("timestamp", Instant.now().toString());
        welcome.put("type", "system");
        welcome.put("message", "WebSocket连接已建立");
        sendJson(session, welcome);
    }

    @Override
    public void afterConnectionClosed(WebSocketSession session, CloseStatus status) {
        sessions.remove(session);
        log.info("WebSocket连接断开，当前连接数: {}", sessions.size());
    }

    public void broadcastLog(String type, String message) {
        Map<String, Object> m = new HashMap<>();
        m.put("timestamp", Instant.now().toString());
        m.put("type", type);
        m.put("message", message);
        broadcastObject(m);
    }

    public void broadcastHeartbeat() {
        broadcastLog("heartbeat", "ping");
    }

    /** 向所有已连接前端推送最新传感器快照，消息 type = "sensor_data"。 */
    public void broadcastSensorData(SensorSnapshot s) {
        Map<String, Object> m = new HashMap<>();
        m.put("type", "sensor_data");
        m.put("timestamp", Instant.now().toString());
        m.put("water_temp",    s.waterTemp());
        m.put("air_temp",      s.airTemp());
        m.put("air_humidity",  s.airHumidity());
        m.put("wqi",           s.wqi());
        m.put("soil_moisture", s.soilMoisture());
        broadcastObject(m);
    }

    private void broadcastObject(Object payload) {
        String json;
        try {
            json = objectMapper.writeValueAsString(payload);
        } catch (Exception e) {
            return;
        }
        for (WebSocketSession session : sessions) {
            if (session.isOpen()) {
                try {
                    session.sendMessage(new TextMessage(json));
                } catch (IOException e) {
                    log.warn("发送WebSocket消息失败: {}", e.getMessage());
                }
            }
        }
    }

    private void sendJson(WebSocketSession session, Object payload) {
        try {
            session.sendMessage(new TextMessage(objectMapper.writeValueAsString(payload)));
        } catch (Exception e) {
            log.warn("发送初始消息失败: {}", e.getMessage());
        }
    }
}
