package com.ruisa.agentweb.web;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;

import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;

import com.ruisa.agentweb.config.AgentProxyProperties;

@RestController
public class CameraProxyController {

    private final AgentProxyProperties props;
    private final HttpClient httpClient =//创建一个HTTP客户端
            HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(10)).build();

    public CameraProxyController(AgentProxyProperties props) {//注入AgentProxyProperties
        this.props = props;
    }

    @GetMapping(value = "/api/v1/camera/mjpeg", produces = "multipart/x-mixed-replace; boundary=frame")
    public ResponseEntity<StreamingResponseBody> mjpeg() {
        String base = props.getBaseUrl().trim().replaceAll("/+$", "");
        String url = base + "/api/v1/camera/mjpeg";

        StreamingResponseBody body =//创建一个响应体，用于流式传输数据
                outputStream -> {
                    HttpRequest req =//创建一个HTTP请求
                            HttpRequest.newBuilder(URI.create(url))
                                    .timeout(Duration.ZERO)
                                    .GET()
                                    .build();
                    try {
                        HttpResponse<InputStream> resp =//发送请求并获取响应
                                httpClient.send(req, HttpResponse.BodyHandlers.ofInputStream());
                        if (resp.statusCode() >= 400) {
                            return;
                        }
                        try (InputStream in = resp.body()) {
                            copyStream(in, outputStream);
                        }
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                    } catch (IOException ignored) {
                    }
                };

        return ResponseEntity.ok()//创建一个响应实体，用于返回响应
                .header(HttpHeaders.CACHE_CONTROL, "no-cache, no-store, must-revalidate")
                .contentType(MediaType.parseMediaType("multipart/x-mixed-replace; boundary=frame"))//设置响应头
                .body(body);
    }

    private static void copyStream(InputStream in, OutputStream out) throws IOException {//将输入流复制到输出流
        byte[] buf = new byte[64 * 1024];
        int n;
        while ((n = in.read(buf)) != -1) {
            out.write(buf, 0, n);
            out.flush();
        }
    }
}
