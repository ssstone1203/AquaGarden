package com.aquagarden.websocket;

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
        sendJson(session, logEntry("system", "WebSocket连接已建立"));
    }

    @Override
    public void afterConnectionClosed(WebSocketSession session, CloseStatus status) {
        sessions.remove(session);
        log.info("WebSocket连接断开，当前连接数: {}", sessions.size());
    }

    public void broadcastLog(String type, String message) {
        broadcastJson(logEntry(type, message));
    }

    public void broadcastHeartbeat() {
        broadcastJson(logEntry("heartbeat", "ping"));
    }

    private Map<String, String> logEntry(String type, String message) {
        Map<String, String> m = new HashMap<>();
        m.put("timestamp", Instant.now().toString());
        m.put("type", type);
        m.put("message", message);
        return m;
    }

    private void broadcastJson(Map<String, String> payload) {
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

    private void sendJson(WebSocketSession session, Map<String, String> payload) {
        try {
            session.sendMessage(new TextMessage(objectMapper.writeValueAsString(payload)));
        } catch (Exception e) {
            log.warn("发送初始消息失败: {}", e.getMessage());
        }
    }
}
