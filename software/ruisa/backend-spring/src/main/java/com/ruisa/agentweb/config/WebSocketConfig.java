package com.ruisa.agentweb.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.web.socket.config.annotation.EnableWebSocket;
import org.springframework.web.socket.config.annotation.WebSocketConfigurer;
import org.springframework.web.socket.config.annotation.WebSocketHandlerRegistry;

import com.ruisa.agentweb.websocket.AgentChatProxyHandler;

@Configuration
@EnableWebSocket
public class WebSocketConfig implements WebSocketConfigurer {//实现WebSocketConfigurer接口，注册WebSocketHandler

    private final AgentChatProxyHandler agentChatProxyHandler;

    public WebSocketConfig(AgentChatProxyHandler agentChatProxyHandler) {
        this.agentChatProxyHandler = agentChatProxyHandler;
    }

    @Override
    public void registerWebSocketHandlers(WebSocketHandlerRegistry registry) {
        registry.addHandler(agentChatProxyHandler, "/api/v1/ws/agent/chat").setAllowedOrigins("*");
    }
}
