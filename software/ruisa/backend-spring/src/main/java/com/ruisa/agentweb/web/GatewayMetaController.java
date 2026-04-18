package com.ruisa.agentweb.web;

import com.ruisa.agentweb.config.AgentProxyProperties;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.view.RedirectView;

import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.Map;

@RestController
public class GatewayMetaController {

    private final AgentProxyProperties props;

    public GatewayMetaController(AgentProxyProperties props) {
        this.props = props;
    }

    @GetMapping("/health")
    public Map<String, Object> health() {
        Map<String, Object> m = new LinkedHashMap<>();
        m.put("status", "ok");
        m.put("service", "ruisa-spring-gateway");
        m.put("upstream", props.getBaseUrl());
        m.put("timestamp", Instant.now().toString());
        return m;
    }

    @GetMapping("/docs")
    public RedirectView docs() {
        return redirect(props.getBaseUrl().trim().replaceAll("/+$", "") + "/docs");
    }

    @GetMapping("/openapi.json")
    public RedirectView openapi() {
        return redirect(props.getBaseUrl().trim().replaceAll("/+$", "") + "/openapi.json");
    }

    private static RedirectView redirect(String url) {
        RedirectView v = new RedirectView(url);
        v.setStatusCode(HttpStatus.FOUND);
        return v;
    }

    /** Optional: HEAD for uptime checks */
    @GetMapping("/api/v1/gateway/ping")
    public ResponseEntity<Void> ping() {
        return ResponseEntity.noContent().build();
    }
}
