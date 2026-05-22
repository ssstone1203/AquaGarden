package com.aquagarden.service;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.net.URLConnection;
import java.util.Map;
import java.util.Set;

@Service
public class CameraProxyService {
    private static final Set<String> MODES = Set.of("rgb", "depth");

    private final String rgbUrl;
    private final String depthUrl;

    public CameraProxyService(
            @Value("${aquagarden.hardware.camera.rgb-url:}") String rgbUrl,
            @Value("${aquagarden.hardware.camera.depth-url:}") String depthUrl) {
        this.rgbUrl = normalize(rgbUrl);
        this.depthUrl = normalize(depthUrl);
    }

    public boolean supports(String mode) {
        return MODES.contains(mode);
    }

    public boolean hasSource(String mode) {
        return !sourceFor(mode).isBlank();
    }

    public Map<String, Object> status() {
        return Map.of(
                "hasRgb", !rgbUrl.isBlank(),
                "hasDepth", !depthUrl.isBlank(),
                "rgbUrlConfigured", !rgbUrl.isBlank(),
                "depthUrlConfigured", !depthUrl.isBlank()
        );
    }

    public void stream(String mode, OutputStream outputStream) throws IOException {
        String source = sourceFor(mode);
        if (source.isBlank()) {
            throw new IOException("camera source is not configured: " + mode);
        }
        URLConnection connection = URI.create(source).toURL().openConnection();
        connection.setConnectTimeout(3000);
        connection.setReadTimeout(0);
        try (InputStream inputStream = connection.getInputStream()) {
            inputStream.transferTo(outputStream);
        }
    }

    private String sourceFor(String mode) {
        return "depth".equals(mode) ? depthUrl : rgbUrl;
    }

    private static String normalize(String value) {
        return value == null ? "" : value.trim();
    }
}
