package com.aquagarden.web;

import com.aquagarden.service.McuCommandService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

/**
 * MCU 水泵命令 API。
 * <p>
 * 前端 POST 入队 → 后端内存暂存 → serial_bridge.py GET 轮询消费 → UART 写入 MCU。
 */
@RestController
public class McuPumpController {

    private final McuCommandService mcuCommandService;

    public McuPumpController(McuCommandService mcuCommandService) {
        this.mcuCommandService = mcuCommandService;
    }

    /**
     * 前端调用：入队一条 MCU 水泵命令。
     * <p>
     * 请求体：
     * <pre>
     * { "action": "start",  "power": 80 }
     * { "action": "stop" }
     * { "action": "set_pwm", "power": 50 }
     * </pre>
     */
    @PostMapping("/api/mcu/pump")
    public ResponseEntity<Map<String, Object>> enqueue(@RequestBody Map<String, Object> body) {
        String action = String.valueOf(body.getOrDefault("action", "")).toLowerCase();
        int power = parsePower(body.get("power"));

        int cmd = switch (action) {
            case "stop"    -> McuCommandService.CMD_STOP;
            case "start"   -> McuCommandService.CMD_START;
            case "set_pwm" -> McuCommandService.CMD_SET_PWM;
            default -> -1;
        };

        if (cmd < 0) {
            return ResponseEntity.badRequest().body(Map.of(
                    "ok", false,
                    "message", "未知 action: " + action + "，支持 stop / start / set_pwm"
            ));
        }

        mcuCommandService.enqueue(cmd, power);
        return ResponseEntity.ok(Map.of(
                "ok", true,
                "action", action,
                "power", power
        ));
    }

    /**
     * serial_bridge.py 轮询：取出并清空待发送命令。
     * <p>
     * 无待发命令时返回 204 No Content。
     * 有待发命令时返回 JSON：{ "cmd": 2, "power": 80, "cmdName": "start" }
     */
    @GetMapping("/api/mcu/pump/pending")
    public ResponseEntity<Map<String, Object>> pollPending() {
        McuCommandService.PendingCommand cmd = mcuCommandService.drain();
        if (cmd == null) {
            return ResponseEntity.noContent().build();
        }
        return ResponseEntity.ok(Map.of(
                "cmd", cmd.cmd(),
                "power", cmd.power(),
                "cmdName", cmd.cmdName()
        ));
    }

    /**
     * 查看当前队列状态（调试用）。
     */
    @GetMapping("/api/mcu/pump/status")
    public ResponseEntity<Map<String, Object>> status() {
        McuCommandService.PendingCommand cmd = mcuCommandService.peek();
        if (cmd == null) {
            return ResponseEntity.ok(Map.of("pending", false));
        }
        return ResponseEntity.ok(Map.of(
                "pending", true,
                "cmd", cmd.cmd(),
                "power", cmd.power(),
                "cmdName", cmd.cmdName()
        ));
    }

    private static int parsePower(Object value) {
        if (value == null) return 0;
        if (value instanceof Number n) return n.intValue();
        try {
            return Integer.parseInt(String.valueOf(value));
        } catch (NumberFormatException e) {
            return 0;
        }
    }
}
