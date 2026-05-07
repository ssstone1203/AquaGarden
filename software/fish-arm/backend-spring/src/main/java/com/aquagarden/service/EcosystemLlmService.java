package com.aquagarden.service;

import com.aquagarden.dto.SensorSnapshot;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * 调用大模型对传感器快照做简短中文综合分析：支持 OpenAI Chat Completions 与 Anthropic Messages。
 * 未配置 API Key 或调用失败时使用基于阈值的本地回退文案。
 */
@Service
public class EcosystemLlmService {

    private static final String PROVIDER_OPENAI = "openai";
    private static final String PROVIDER_ANTHROPIC = "anthropic";

    private final HttpClient httpClient;
    private final ObjectMapper objectMapper;
    private final String baseUrl;
    private final String apiKey;
    private final String model;
    private final boolean llmConfigured;
    private final boolean llmFeatureEnabled;
    private final boolean hasApiKey;
    private final String provider;
    private final String anthropicVersion;
    private final int maxTokens;

    /** LLM HTTP 单次调用结果（成功带文本，否则带可读错误摘要） */
    private static final class LlmHttpOutcome {
        final String text;
        final String errorHint;

        private LlmHttpOutcome(String text, String errorHint) {
            this.text = text;
            this.errorHint = errorHint;
        }

        static LlmHttpOutcome ok(String t) {
            return new LlmHttpOutcome(t, null);
        }

        static LlmHttpOutcome fail(String hint) {
            return new LlmHttpOutcome(null, hint);
        }
    }

    public EcosystemLlmService(
            ObjectMapper objectMapper,
            @Value("${aquagarden.llm.provider:openai}") String provider,
            @Value("${aquagarden.llm.base-url:https://api.openai.com/v1}") String baseUrl,
            @Value("${aquagarden.llm.api-key:}") String apiKey,
            @Value("${aquagarden.llm.model:gpt-4o-mini}") String model,
            @Value("${aquagarden.llm.enabled:false}") boolean llmFeatureEnabled,
            @Value("${aquagarden.llm.anthropic-version:2023-06-01}") String anthropicVersion,
            @Value("${aquagarden.llm.max-tokens:768}") int maxTokens) {
        this.objectMapper = objectMapper;
        this.baseUrl = stripTrailingSlash(baseUrl);
        this.apiKey = apiKey == null ? "" : apiKey.trim();
        this.model = model == null || model.isBlank() ? "gpt-4o-mini" : model;
        this.llmFeatureEnabled = llmFeatureEnabled;
        this.hasApiKey = !this.apiKey.isBlank();
        this.llmConfigured = llmFeatureEnabled && this.hasApiKey;
        this.provider = normalizeProvider(provider);
        this.anthropicVersion = anthropicVersion == null || anthropicVersion.isBlank()
                ? "2023-06-01"
                : anthropicVersion.trim();
        this.maxTokens = maxTokens > 0 ? maxTokens : 768;
        this.httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(8))
                .build();
    }

    public Map<String, Object> analyze(SensorSnapshot snapshot, boolean fromHardware) {
        if (!llmConfigured) {
            String skipMsg;
            if (!llmFeatureEnabled) {
                skipMsg = "大模型功能未启用（aquagarden.llm.enabled=false），已使用本地规则分析。";
            } else if (!hasApiKey) {
                skipMsg = "未配置 API Key（如环境变量 ANTHROPIC_API_KEY / OPENAI_API_KEY），已使用本地规则分析。";
            } else {
                skipMsg = "大模型不可用，已使用本地规则分析。";
            }
            return buildFallback(snapshot, fromHardware, "skipped", skipMsg);
        }

        try {
            LlmHttpOutcome out = PROVIDER_ANTHROPIC.equals(provider)
                    ? callAnthropicMessages(snapshot, fromHardware)
                    : callChatCompletions(snapshot, fromHardware);

            if (out.text != null && !out.text.isBlank()) {
                Map<String, Object> ok = baseOkResponse();
                ok.put("source", "llm");
                ok.put("provider", provider);
                ok.put("model", model);
                ok.put("analysis", out.text.trim());
                ok.put("llmOk", true);
                ok.put("llmStatus", "ok");
                ok.put("llmMessage", "大模型已成功返回分析内容。");
                return ok;
            }

            String err = out.errorHint != null ? out.errorHint : "上游返回为空或无法解析正文";
            return buildFallback(snapshot, fromHardware, "error",
                    "大模型调用未成功：" + err + " 以下为本地规则回退结论。");

        } catch (Exception e) {
            String msg = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
            if (msg.length() > 220) {
                msg = msg.substring(0, 220) + "…";
            }
            return buildFallback(snapshot, fromHardware, "error",
                    "大模型调用异常：" + msg + " 以下为本地规则回退结论。");
        }
    }

    private Map<String, Object> buildFallback(
            SensorSnapshot snapshot, boolean fromHardware, String llmStatus, String llmMessage) {
        Map<String, Object> m = baseOkResponse();
        m.put("source", "fallback");
        m.put("provider", "none");
        m.put("model", "rule-based");
        m.put("analysis", ruleBasedAnalysis(snapshot, fromHardware));
        m.put("llmOk", false);
        m.put("llmStatus", llmStatus);
        m.put("llmMessage", llmMessage);
        return m;
    }

    private static Map<String, Object> baseOkResponse() {
        Map<String, Object> m = new LinkedHashMap<>();
        m.put("ok", true);
        return m;
    }

    private static String normalizeProvider(String p) {
        if (p == null || p.isBlank()) {
            return PROVIDER_OPENAI;
        }
        String v = p.trim().toLowerCase();
        return PROVIDER_ANTHROPIC.equals(v) ? PROVIDER_ANTHROPIC : PROVIDER_OPENAI;
    }

    /** Anthropic Claude Messages API：POST /v1/messages */
    private LlmHttpOutcome callAnthropicMessages(SensorSnapshot s, boolean fromHardware) throws Exception {
        String sys = "你是水族箱与智慧盆栽一体化生态系统的助手。根据传感器数据用中文给出简短、可执行的建议，控制在约 200 字内，分「状态」「风险」「建议」三层表述，语气专业友好。";
        String user = """
                数据来源：%s
                水温 %.1f °C，气温 %.1f °C，空气湿度 %.1f %%RH，水质综合指数 %.0f / 100，土壤湿度 %.0f %%。
                """.formatted(fromHardware ? "硬件实时采样" : "演示/默认值", s.waterTemp(), s.airTemp(), s.airHumidity(), s.wqi(), s.soilMoisture());

        Map<String, Object> body = new LinkedHashMap<>();
        body.put("model", model);
        body.put("max_tokens", maxTokens);
        body.put("temperature", 0.4);
        body.put("system", sys);
        body.put("messages", List.of(Map.of("role", "user", "content", user)));

        String json = objectMapper.writeValueAsString(body);
        String endpoint = baseUrl.endsWith("/v1") ? baseUrl + "/messages" : baseUrl + "/v1/messages";
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(endpoint))
                .timeout(Duration.ofSeconds(60))
                .header("Content-Type", "application/json")
                .header("x-api-key", apiKey)
                .header("anthropic-version", anthropicVersion)
                .POST(HttpRequest.BodyPublishers.ofString(json))
                .build();

        HttpResponse<String> resp = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
        String respBody = resp.body() == null ? "" : resp.body();
        if (resp.statusCode() < 200 || resp.statusCode() >= 300) {
            return LlmHttpOutcome.fail(summarizeUpstreamError(respBody, resp.statusCode()));
        }
        JsonNode root = objectMapper.readTree(respBody);
        if ("error".equals(root.path("type").asText())) {
            return LlmHttpOutcome.fail(summarizeUpstreamError(respBody, resp.statusCode()));
        }
        JsonNode content = root.path("content");
        if (!content.isArray() || content.isEmpty()) {
            return LlmHttpOutcome.fail("响应中无 content 数组或为空");
        }
        for (JsonNode block : content) {
            if ("text".equals(block.path("type").asText())) {
                String t = block.path("text").asText(null);
                if (t != null && !t.isBlank()) {
                    return LlmHttpOutcome.ok(t);
                }
            }
        }
        return LlmHttpOutcome.fail("响应中无 text 内容块");
    }

    private LlmHttpOutcome callChatCompletions(SensorSnapshot s, boolean fromHardware) throws Exception {
        String sys = "你是水族箱与智慧盆栽一体化生态系统的助手。根据传感器数据用中文给出简短、可执行的建议，控制在约 200 字内，分「状态」「风险」「建议」三层表述，语气专业友好。";
        String user = """
                数据来源：%s
                水温 %.1f °C，气温 %.1f °C，空气湿度 %.1f %%RH，水质综合指数 %.0f / 100，土壤湿度 %.0f %%。
                """.formatted(fromHardware ? "硬件实时采样" : "演示/默认值", s.waterTemp(), s.airTemp(), s.airHumidity(), s.wqi(), s.soilMoisture());

        List<Map<String, Object>> messages = new ArrayList<>();
        messages.add(Map.of("role", "system", "content", sys));
        messages.add(Map.of("role", "user", "content", user));

        Map<String, Object> body = Map.of(
                "model", model,
                "temperature", 0.4,
                "messages", messages
        );

        String json = objectMapper.writeValueAsString(body);
        HttpRequest.Builder rb = HttpRequest.newBuilder()
                .uri(URI.create(baseUrl + "/chat/completions"))
                .timeout(Duration.ofSeconds(45))
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(json));
        if (!apiKey.isBlank()) {
            rb.header("Authorization", "Bearer " + apiKey);
        }

        HttpResponse<String> resp = httpClient.send(rb.build(), HttpResponse.BodyHandlers.ofString());
        String respBody = resp.body() == null ? "" : resp.body();
        if (resp.statusCode() < 200 || resp.statusCode() >= 300) {
            return LlmHttpOutcome.fail(summarizeUpstreamError(respBody, resp.statusCode()));
        }
        JsonNode root = objectMapper.readTree(respBody);
        JsonNode choices = root.path("choices");
        if (!choices.isArray() || choices.isEmpty()) {
            return LlmHttpOutcome.fail("choices 为空，无法读取模型输出");
        }
        String t = choices.get(0).path("message").path("content").asText(null);
        if (t == null || t.isBlank()) {
            return LlmHttpOutcome.fail("模型返回的 message.content 为空");
        }
        return LlmHttpOutcome.ok(t);
    }

    /** 从 OpenAI / Anthropic 风格错误 JSON 中提取简短说明 */
    private String summarizeUpstreamError(String body, int status) {
        String prefix = "HTTP " + status;
        if (body == null || body.isBlank()) {
            return prefix;
        }
        try {
            JsonNode root = objectMapper.readTree(body);
            JsonNode err = root.path("error");
            if (err.isTextual()) {
                return prefix + ": " + clampApiMsg(err.asText());
            }
            if (err.isObject()) {
                String type = err.path("type").asText("");
                String msg = err.path("message").asText("");
                if (!msg.isBlank()) {
                    String extra = type.isBlank() ? msg : (type + " — " + msg);
                    return prefix + ": " + clampApiMsg(extra);
                }
            }
        } catch (Exception ignored) {
            // 非 JSON，走下方截断原文
        }
        return prefix + ": " + clampApiMsg(body);
    }

    private static String clampApiMsg(String s) {
        if (s == null) {
            return "";
        }
        String t = s.replace("\r\n", " ").replace('\n', ' ').trim();
        if (t.length() > 280) {
            return t.substring(0, 280) + "…";
        }
        return t;
    }

    private static String ruleBasedAnalysis(SensorSnapshot s, boolean fromHardware) {
        List<String> parts = new ArrayList<>();
        parts.add(fromHardware ? "【数据来源】当前为硬件采样。" : "【数据来源】设备未上报或处于演示模式，以下为基于典型阈值的参考判断。");

        if (s.waterTemp() < 18) {
            parts.add("【状态】水温偏低，多数热带鱼代谢会放缓。");
        } else if (s.waterTemp() > 30) {
            parts.add("【状态】水温偏高，溶氧下降与致病菌风险上升。");
        } else {
            parts.add("【状态】水温处于常见观赏鱼舒适区间。");
        }

        if (s.wqi() < 50) {
            parts.add("【风险】水质指数偏低，需关注过滤、投喂量与换水频率。");
        } else if (s.wqi() >= 80) {
            parts.add("【风险】水质指数良好，继续保持定期维护即可。");
        } else {
            parts.add("【风险】水质中等，建议观察浑浊度与鱼类摄食行为。");
        }

        if (s.soilMoisture() < 35) {
            parts.add("【建议】基质偏干，可适度补水或检查鱼缸废水灌溉链路。");
        } else if (s.soilMoisture() > 85) {
            parts.add("【建议】基质过湿，注意根系透气与霉菌风险。");
        } else {
            parts.add("【建议】基质湿度适中，可按日程 light 投喂与机械臂维护。");
        }

        parts.add("【建议】若长期离线，请检查串口桥接脚本与 MCU 帧是否持续 POST 到 /api/sensors/ingest。");
        return String.join("", parts);
    }

    private static String stripTrailingSlash(String value) {
        if (value == null || value.isBlank()) {
            return "https://api.openai.com/v1";
        }
        return value.endsWith("/") ? value.substring(0, value.length() - 1) : value;
    }
}
