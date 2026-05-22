package com.aquagarden.web;

import com.aquagarden.dto.TankDetection;
import com.aquagarden.service.TankVideoFrameService;
import com.fasterxml.jackson.databind.JsonNode;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;

import javax.imageio.ImageIO;
import java.awt.*;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicReference;

@RestController
public class VideoController {

    private static final String BOUNDARY = "frame";
    private static final MediaType MJPEG = MediaType.parseMediaType("multipart/x-mixed-replace; boundary=" + BOUNDARY);

    private final TankVideoFrameService tankVideoFrameService;
    private final AtomicReference<List<TankDetection>> tankDetections = new AtomicReference<>(List.of());
    private final AtomicReference<Long> tankDetectionsAt = new AtomicReference<>(0L);

    public VideoController(TankVideoFrameService tankVideoFrameService) {
        this.tankVideoFrameService = tankVideoFrameService;
    }

    @GetMapping(value = "/api/video/robot", produces = "multipart/x-mixed-replace; boundary=" + BOUNDARY)
    public ResponseEntity<StreamingResponseBody> robot() {
        return generatedStream("机械臂", "Camera Feed");
    }

    @GetMapping(value = "/api/video/tank", produces = "multipart/x-mixed-replace; boundary=" + BOUNDARY)
    public ResponseEntity<StreamingResponseBody> tank() {
        return tankStream();
    }

    /**
     * 当前鱼缸 JPEG 单帧（供延时摄影等低频抓取，减轻 MJPEG 多连接压力）。
     */
    @GetMapping(value = "/api/video/tank/snapshot", produces = MediaType.IMAGE_JPEG_VALUE)
    public ResponseEntity<byte[]> tankSnapshot() {
        byte[] frame = tankVideoFrameService.latestFrame();
        if (frame == null) {
            return ResponseEntity.notFound().build();
        }
        try {
            byte[] out = maybeDrawDetections(frame);
            return ResponseEntity.ok(out);
        } catch (Exception e) {
            return ResponseEntity.ok(frame);
        }
    }

    @GetMapping("/api/video/tank/status")
    public Map<String, Object> tankStatus() {
        return tankVideoFrameService.status(tankDetections.get().size());
    }

    /**
     * 外部推理进程（如 YOLO）POST 检测框列表；归一化坐标 0~1，叠加在鱼缸 MJPEG 上。
     */
    @PostMapping("/api/video/tank/detections")
    public ResponseEntity<Map<String, Object>> ingestDetections(@RequestBody JsonNode body) {
        try {
            List<TankDetection> list = parseDetections(body);
            tankDetections.set(list);
            tankDetectionsAt.set(System.currentTimeMillis());
            return ResponseEntity.ok(Map.of("ok", true, "count", list.size()));
        } catch (Exception e) {
            return ResponseEntity.badRequest().body(Map.of("ok", false, "message", e.getMessage()));
        }
    }

    @PostMapping(value = "/api/video/tank/ingest", consumes = MediaType.IMAGE_JPEG_VALUE)
    public ResponseEntity<Map<String, Object>> ingestTankFrame(@RequestBody byte[] frame) {
        if (!tankVideoFrameService.updateFrame(frame)) {
            return ResponseEntity.badRequest().body(Map.of(
                    "ok", false,
                    "message", "invalid jpeg frame"
            ));
        }

        return ResponseEntity.ok(Map.of(
                "ok", true,
                "seq", tankVideoFrameService.frameSeq(),
                "bytes", frame.length
        ));
    }

    private List<TankDetection> parseDetections(JsonNode body) {
        JsonNode arr = body.path("detections");
        if (!arr.isArray()) {
            throw new IllegalArgumentException("detections must be array");
        }
        List<TankDetection> out = new ArrayList<>();
        for (JsonNode n : arr) {
            String label = n.path("label").asText("object");
            double x = readNorm(n, "x");
            double y = readNorm(n, "y");
            double w = readNorm(n, "width", "w");
            double h = readNorm(n, "height", "h");
            double score = n.path("score").isNumber() ? n.path("score").asDouble(0) : n.path("confidence").asDouble(0);
            out.add(new TankDetection(label, clamp01(x), clamp01(y), clamp01(w), clamp01(h), score));
        }
        return out;
    }

    private static double readNorm(JsonNode n, String primary, String... alternates) {
        if (n.has(primary) && n.path(primary).isNumber()) {
            return n.path(primary).asDouble();
        }
        for (String a : alternates) {
            if (n.has(a) && n.path(a).isNumber()) {
                return n.path(a).asDouble();
            }
        }
        return 0;
    }

    private static double clamp01(double v) {
        return Math.max(0.0, Math.min(1.0, v));
    }

    private ResponseEntity<StreamingResponseBody> tankStream() {
        StreamingResponseBody body = outputStream -> {
            byte[] boundaryEnd = "\r\n".getBytes(StandardCharsets.UTF_8);
            long lastSeq = -1L;
            try {
                while (!Thread.currentThread().isInterrupted()) {
                    long seq = tankVideoFrameService.frameSeq();
                    byte[] frame = tankVideoFrameService.latestFrame();
                    if (frame == null) {
                        frame = generatedJpeg("鱼缸", "Waiting for serial camera frame");
                    } else if (seq == lastSeq) {
                        pause(50);
                        continue;
                    }

                    byte[] payload = maybeDrawDetections(frame);
                    byte[] boundaryPrefix = ("--" + BOUNDARY
                            + "\r\nContent-Type: image/jpeg"
                            + "\r\nContent-Length: " + payload.length
                            + "\r\n\r\n").getBytes(StandardCharsets.UTF_8);
                    outputStream.write(boundaryPrefix);
                    outputStream.write(payload);
                    outputStream.write(boundaryEnd);
                    outputStream.flush();
                    lastSeq = seq;
                    pause(frame == tankVideoFrameService.latestFrame() ? 20 : 500);
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } catch (Exception ignored) {
                // client disconnect
            }
        };
        return ResponseEntity.ok().contentType(MJPEG).body(body);
    }

    private byte[] maybeDrawDetections(byte[] jpeg) {
        List<TankDetection> dets = tankDetections.get();
        if (dets == null || dets.isEmpty()) {
            return jpeg;
        }
        long ageMs = System.currentTimeMillis() - tankDetectionsAt.get();
        if (ageMs > 1500) {
            return jpeg;
        }
        try {
            BufferedImage img = ImageIO.read(new ByteArrayInputStream(jpeg));
            if (img == null) {
                return jpeg;
            }
            int iw = img.getWidth();
            int ih = img.getHeight();
            Graphics2D g = img.createGraphics();
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            g.setStroke(new BasicStroke(2f));
            g.setFont(new Font(Font.SANS_SERIF, Font.BOLD, Math.max(12, ih / 28)));
            for (TankDetection d : dets) {
                int x = (int) Math.round(d.x() * iw);
                int y = (int) Math.round(d.y() * ih);
                int w = (int) Math.round(d.width() * iw);
                int h = (int) Math.round(d.height() * ih);
                g.setColor(new Color(0, 255, 140, 220));
                g.drawRect(x, y, Math.max(1, w), Math.max(1, h));
                String cap = d.label();
                if (d.score() > 0) {
                    double pct = d.score() <= 1.0 ? d.score() * 100.0 : d.score();
                    cap = cap + String.format(" %.0f%%", pct);
                }
                g.setColor(new Color(0, 0, 0, 140));
                g.fillRect(x, Math.max(0, y - 18), Math.min(iw - x, cap.length() * 7 + 8), 18);
                g.setColor(new Color(0, 255, 170));
                g.drawString(cap, x + 4, Math.max(12, y - 4));
            }
            g.dispose();
            ByteArrayOutputStream jpg = new ByteArrayOutputStream();
            ImageIO.write(img, "jpg", jpg);
            return jpg.toByteArray();
        } catch (Exception e) {
            return jpeg;
        }
    }

    private ResponseEntity<StreamingResponseBody> generatedStream(String label, String subtitle) {
        StreamingResponseBody body = outputStream -> {
            byte[] boundaryPrefix = ("--" + BOUNDARY + "\r\nContent-Type: image/jpeg\r\n\r\n").getBytes(StandardCharsets.UTF_8);
            byte[] boundaryEnd = "\r\n".getBytes(StandardCharsets.UTF_8);
            try {
                while (!Thread.currentThread().isInterrupted()) {
                    outputStream.write(boundaryPrefix);
                    outputStream.write(generatedJpeg(label, subtitle));
                    outputStream.write(boundaryEnd);
                    outputStream.flush();
                    pause(100);
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } catch (Exception ignored) {
                // client disconnect
            }
        };
        return ResponseEntity.ok().contentType(MJPEG).body(body);
    }

    private static byte[] generatedJpeg(String label, String subtitle) throws Exception {
        Random rnd = new Random();
        BufferedImage img = new BufferedImage(640, 480, BufferedImage.TYPE_INT_RGB);
        Graphics2D g = img.createGraphics();
        g.setColor(new Color(rnd.nextInt(200), rnd.nextInt(200), rnd.nextInt(200)));
        g.fillRect(0, 0, 640, 480);
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setColor(Color.WHITE);
        g.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 20));
        String time = LocalTime.now().format(DateTimeFormatter.ofPattern("HH:mm:ss"));
        g.drawString("Time: " + time, 10, 30);
        g.setColor(new Color(0, 255, 100));
        g.drawString(label + " · " + subtitle, 10, 60);
        g.dispose();

        ByteArrayOutputStream jpg = new ByteArrayOutputStream();
        ImageIO.write(img, "jpg", jpg);
        return jpg.toByteArray();
    }

    private static void pause(long millis) throws InterruptedException {
        Thread.sleep(millis);
    }
}
