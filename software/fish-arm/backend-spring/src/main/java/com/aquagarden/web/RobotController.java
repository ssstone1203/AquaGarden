package com.aquagarden.web;

import com.aquagarden.dto.RobotControlRequest;
import com.aquagarden.service.SystemStateService;
import com.aquagarden.websocket.LogWebSocketHandler;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.server.ResponseStatusException;

import jakarta.validation.Valid;
import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.Set;

@RestController
public class RobotController {

    private static final Set<String> VALID = Set.of("up", "down", "left", "right", "forward", "backward");

    private final SystemStateService systemStateService;
    private final LogWebSocketHandler logWebSocketHandler;

    public RobotController(SystemStateService systemStateService, LogWebSocketHandler logWebSocketHandler) {
        this.systemStateService = systemStateService;
        this.logWebSocketHandler = logWebSocketHandler;
    }

    @PostMapping("/api/robot/control")
    public Map<String, Object> control(@Valid @RequestBody RobotControlRequest body) {
        String dir = body.direction();
        if (!VALID.contains(dir)) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid direction");
        }
        Map<String, Integer> position = systemStateService.moveRobot(dir);
        String msg = "机械臂移动: " + dir + ", 位置: " + position;
        logWebSocketHandler.broadcastLog("robot", msg);
        return Map.of("status", "success", "position", position);
    }

    /**
     * 获取机械臂实时状态：舵机角度（J1-J6）、当前任务、连接状态、运行时长。
     * 前端机械臂终端面板轮询此接口（建议间隔 1s）。
     */
    @GetMapping("/api/robot/status")
    public Map<String, Object> status() {
        int[] angles = systemStateService.getServoAngles();
        Map<String, Integer> pos = systemStateService.getRobotPosition();
        return Map.of(
                "connected", true,
                "currentTask", "待命",
                "uptime", "03:24:15",
                "position", pos,
                "servoAngles", List.of(angles[0], angles[1], angles[2], angles[3], angles[4], angles[5]),
                "timestamp", Instant.now().toString()
        );
    }
}
