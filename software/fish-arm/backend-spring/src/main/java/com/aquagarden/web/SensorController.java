package com.aquagarden.web;

import com.aquagarden.dto.SensorSnapshot;
import com.aquagarden.service.SystemStateService;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

@RestController
public class SensorController {

    private final SystemStateService systemStateService;

    public SensorController(SystemStateService systemStateService) {
        this.systemStateService = systemStateService;
    }

    @GetMapping("/api/sensors")
    public Map<String, Object> sensors() {
        SensorSnapshot s = systemStateService.readSensorsWithNoise();
        return Map.of(
                "temperature", s.temperature(),
                "ph", s.ph(),
                "oxygen", s.oxygen(),
                "turbidity", s.turbidity()
        );
    }
}
