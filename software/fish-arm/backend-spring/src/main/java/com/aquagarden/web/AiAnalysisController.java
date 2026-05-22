package com.aquagarden.web;

import com.aquagarden.dto.AiChatRequest;
import com.aquagarden.dto.SensorSnapshot;
import com.aquagarden.service.EcosystemLlmService;
import com.aquagarden.service.SystemStateService;
import jakarta.validation.Valid;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

@RestController
public class AiAnalysisController {

    private final SystemStateService systemStateService;
    private final EcosystemLlmService llmService;

    public AiAnalysisController(SystemStateService systemStateService, EcosystemLlmService llmService) {
        this.systemStateService = systemStateService;
        this.llmService = llmService;
    }

    /**
     * 基于当前内存中的传感器快照（硬件优先，否则稳定演示值）请求大模型或规则回退分析。
     */
    @PostMapping("/api/ai/ecosystem-analysis")
    public Map<String, Object> ecosystemAnalysis() {
        SensorSnapshot snap = systemStateService.readSensorsWithNoise();
        boolean hw = systemStateService.hasHardwareSnapshot();
        return llmService.analyze(snap, hw);
    }

    @PostMapping("/api/ai/chat")
    public Map<String, Object> chat(@Valid @RequestBody AiChatRequest request) {
        SensorSnapshot snap = systemStateService.readSensorsWithNoise();
        boolean hw = systemStateService.hasHardwareSnapshot();
        return llmService.chat(snap, hw, request.message(), request.history());
    }
}
