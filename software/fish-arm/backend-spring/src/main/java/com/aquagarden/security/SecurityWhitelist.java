package com.aquagarden.security;

import org.springframework.util.AntPathMatcher;

import java.util.Arrays;

/**
 * 统一维护安全放行路径，避免 SecurityConfig 与 JwtAuthFilter 配置漂移。
 */
public final class SecurityWhitelist {
    private static final AntPathMatcher PATH_MATCHER = new AntPathMatcher();

    private SecurityWhitelist() {
    }

    public static final String[] PUBLIC_ENDPOINTS = {
            "/api/register",
            "/api/login",
            "/api/users",
            "/api/video/**",
            "/api/aqua/video/**",
            "/api/debug/whoami",
            "/api/sensor/latest",
            "/api/sensor/upload",
            "/api/sensors",
            "/api/sensors/ingest",
            "/api/sensors/history",
            "/api/robot/status",
            "/api/mcu/pump/pending",
            "/api/mcu/pump/status",
            "/ws/**",
            "/error"
    };

    public static boolean isJwtSkipped(String uri) {
        return Arrays.stream(PUBLIC_ENDPOINTS).anyMatch(pattern -> PATH_MATCHER.match(pattern, uri));
    }
}
