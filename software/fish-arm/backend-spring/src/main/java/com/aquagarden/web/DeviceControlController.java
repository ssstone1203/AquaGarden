package com.aquagarden.web;

import com.aquagarden.service.AquaBridgeService;
import com.fasterxml.jackson.core.JsonProcessingException;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

@RestController
public class DeviceControlController {

    private final AquaBridgeService bridgeService;

    public DeviceControlController(AquaBridgeService bridgeService) {
        this.bridgeService = bridgeService;
    }

    /**
     * 短时启动水泵（由树莓派 Bridge 执行；未实现时返回明确错误信息）。
     * 请求体：{ "seconds": 5 }，范围 1~120。
     */
    @PostMapping("/api/control/pump")
    public ResponseEntity<Map<String, Object>> pump(@RequestBody Map<String, Object> body)
            throws JsonProcessingException, InterruptedException {
        int seconds = 5;
        Object raw = body == null ? null : body.get("seconds");
        if (raw instanceof Number n) {
            seconds = n.intValue();
        } else if (raw != null) {
            try {
                seconds = Integer.parseInt(String.valueOf(raw));
            } catch (NumberFormatException ignored) {
                seconds = 5;
            }
        }
        seconds = Math.max(1, Math.min(120, seconds));

        try {
            AquaBridgeService.BridgeResponse r = bridgeService.pumpPulse(seconds);
            if (r.statusCode() >= 200 && r.statusCode() < 300) {
                return ResponseEntity.ok(Map.of("ok", true, "seconds", seconds, "bridgeStatus", r.statusCode()));
            }
            if (r.statusCode() == 404) {
                return ResponseEntity.ok(Map.of(
                        "ok", false,
                        "seconds", seconds,
                        "message", "树莓派 Bridge 未实现 POST /api/pump/pulse，请在 Bridge 侧增加水泵控制。"
                ));
            }
            String msg = "Bridge 拒绝水泵请求 HTTP " + r.statusCode();
            if (r.statusCode() == 503) {
                try {
                    Map<String, Object> m = bridgeService.parseMap(r.body());
                    Object mObj = m.get("message");
                    if (mObj != null) {
                        msg = String.valueOf(mObj);
                    }
                } catch (Exception ignored) {
                }
            }
            return ResponseEntity.ok(Map.of("ok", false, "seconds", seconds, "message", msg));
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return ResponseEntity.ok(Map.of(
                    "ok", false,
                    "seconds", seconds,
                    "message", "Bridge 请求被中断"
            ));
        }
    }
}
