package com.aquagarden.web;

import com.aquagarden.service.AquaBridgeService;
import com.aquagarden.service.HardwareSerialService;
import com.aquagarden.service.SerialPumpBridgeService;
import com.fasterxml.jackson.core.JsonProcessingException;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

@RestController
public class DeviceControlController {

    private final AquaBridgeService bridgeService;
    private final HardwareSerialService hardwareSerialService;
    private final SerialPumpBridgeService serialPumpBridgeService;

    public DeviceControlController(AquaBridgeService bridgeService,
                                   HardwareSerialService hardwareSerialService,
                                   SerialPumpBridgeService serialPumpBridgeService) {
        this.bridgeService = bridgeService;
        this.hardwareSerialService = hardwareSerialService;
        this.serialPumpBridgeService = serialPumpBridgeService;
    }

    /**
     * 短时启动水泵。启用后端串口时由 Spring Boot 直接写 MCU 串口，否则保留下游 HTTP 设备兼容。
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
        int pwm = 80;
        Object rawPwm = body == null ? null : body.get("pwm");
        if (rawPwm instanceof Number n) {
            pwm = n.intValue();
        } else if (rawPwm != null) {
            try {
                pwm = Integer.parseInt(String.valueOf(rawPwm));
            } catch (NumberFormatException ignored) {
                pwm = 80;
            }
        }
        pwm = Math.max(0, Math.min(100, pwm));

        try {
            AquaBridgeService.BridgeResponse r = hardwareSerialService.isEnabled()
                    ? hardwareSerialService.pumpPulse(seconds, pwm)
                    : serialPumpBridgeService.isEnabled()
                    ? serialPumpBridgeService.pumpPulse(seconds, pwm)
                    : bridgeService.pumpPulse(seconds, pwm);
            if (r.statusCode() >= 200 && r.statusCode() < 300) {
                return ResponseEntity.ok(Map.of("ok", true, "seconds", seconds, "pwm", pwm, "status", r.statusCode()));
            }
            if (r.statusCode() == 404) {
                return ResponseEntity.ok(Map.of(
                        "ok", false,
                        "seconds", seconds,
                        "message", "水泵脉冲控制未实现。启用 aquagarden.hardware.serial.enabled 或配置下游设备。"
                ));
            }
            String msg = "水泵请求失败 HTTP " + r.statusCode();
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
                    "message", "水泵请求被中断"
            ));
        }
    }
}
