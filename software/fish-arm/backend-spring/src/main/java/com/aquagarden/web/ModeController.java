package com.aquagarden.web;

import com.aquagarden.dto.ModeRequest;
import com.aquagarden.service.SystemStateService;
import com.aquagarden.websocket.LogWebSocketHandler;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.server.ResponseStatusException;

import jakarta.validation.Valid;
import java.util.Map;
import java.util.Set;

@RestController
public class ModeController {

    private static final Set<String> VALID = Set.of("service", "demo");

    private final SystemStateService systemStateService;
    private final LogWebSocketHandler logWebSocketHandler;

    public ModeController(SystemStateService systemStateService, LogWebSocketHandler logWebSocketHandler) {
        this.systemStateService = systemStateService;
        this.logWebSocketHandler = logWebSocketHandler;
    }

    @PostMapping("/api/mode")
    public Map<String, Object> setMode(@Valid @RequestBody ModeRequest body) {
        if (!VALID.contains(body.mode())) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid mode");
        }
        systemStateService.setMode(body.mode());
        logWebSocketHandler.broadcastLog("system", "模式切换: " + body.mode());
        return Map.of("status", "success", "mode", systemStateService.getMode());
    }

    @GetMapping("/api/mode")
    public Map<String, String> getMode() {
        return Map.of("mode", systemStateService.getMode());
    }
}
