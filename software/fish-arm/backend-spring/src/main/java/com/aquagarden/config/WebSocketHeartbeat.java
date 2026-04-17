package com.aquagarden.config;

import com.aquagarden.websocket.LogWebSocketHandler;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

@Component
public class WebSocketHeartbeat {

    private final LogWebSocketHandler logWebSocketHandler;

    public WebSocketHeartbeat(LogWebSocketHandler logWebSocketHandler) {
        this.logWebSocketHandler = logWebSocketHandler;
    }

    @Scheduled(fixedRate = 30_000)
    public void sendPing() {
        logWebSocketHandler.broadcastHeartbeat();
    }
}
