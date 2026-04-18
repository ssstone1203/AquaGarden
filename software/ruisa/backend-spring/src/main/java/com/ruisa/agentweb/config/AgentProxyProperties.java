package com.ruisa.agentweb.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "ruisa.agent")
public class AgentProxyProperties {

    /**
     * Python FastAPI base URL (no trailing slash), e.g. http://127.0.0.1:8000
     */
    private String baseUrl = "http://127.0.0.1:8000";

    public String getBaseUrl() {
        return baseUrl;
    }

    public void setBaseUrl(String baseUrl) {
        this.baseUrl = baseUrl;
    }
}
