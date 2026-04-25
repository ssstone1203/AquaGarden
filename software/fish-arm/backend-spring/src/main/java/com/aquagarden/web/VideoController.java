package com.aquagarden.web;

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
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

@RestController
public class VideoController {

    private static final String BOUNDARY = "frame";
    private static final MediaType MJPEG = MediaType.parseMediaType("multipart/x-mixed-replace; boundary=" + BOUNDARY);
    private static final int MAX_JPEG_BYTES = 1024 * 1024;

    private final AtomicReference<byte[]> latestTankFrame = new AtomicReference<>();
    private final AtomicLong latestTankFrameAt = new AtomicLong(0L);
    private final AtomicLong tankFrameSeq = new AtomicLong(0L);

    @GetMapping(value = "/api/video/robot", produces = "multipart/x-mixed-replace; boundary=" + BOUNDARY)
    public ResponseEntity<StreamingResponseBody> robot() {
        return generatedStream("机械臂", "Camera Feed");
    }

    @GetMapping(value = "/api/video/tank", produces = "multipart/x-mixed-replace; boundary=" + BOUNDARY)
    public ResponseEntity<StreamingResponseBody> tank() {
        return tankStream();
    }

    @GetMapping("/api/video/tank/status")
    public Map<String, Object> tankStatus() {
        byte[] frame = latestTankFrame.get();
        return Map.of(
                "hasFrame", frame != null,
                "seq", tankFrameSeq.get(),
                "updatedAt", latestTankFrameAt.get(),
                "bytes", frame == null ? 0 : frame.length
        );
    }

    @PostMapping(value = "/api/video/tank/ingest", consumes = MediaType.IMAGE_JPEG_VALUE)
    public ResponseEntity<Map<String, Object>> ingestTankFrame(@RequestBody byte[] frame) {
        if (frame == null || frame.length < 4 || frame.length > MAX_JPEG_BYTES || !isJpeg(frame)) {
            return ResponseEntity.badRequest().body(Map.of(
                    "ok", false,
                    "message", "invalid jpeg frame"
            ));
        }

        latestTankFrame.set(frame);
        latestTankFrameAt.set(System.currentTimeMillis());
        long seq = tankFrameSeq.incrementAndGet();
        return ResponseEntity.ok(Map.of(
                "ok", true,
                "seq", seq,
                "bytes", frame.length
        ));
    }

    private ResponseEntity<StreamingResponseBody> tankStream() {
        StreamingResponseBody body = outputStream -> {
            byte[] boundaryPrefix = ("--" + BOUNDARY + "\r\nContent-Type: image/jpeg\r\n\r\n").getBytes(StandardCharsets.UTF_8);
            byte[] boundaryEnd = "\r\n".getBytes(StandardCharsets.UTF_8);
            long lastSeq = -1L;
            try {
                while (!Thread.currentThread().isInterrupted()) {
                    long seq = tankFrameSeq.get();
                    byte[] frame = latestTankFrame.get();
                    if (frame == null) {
                        frame = generatedJpeg("鱼缸", "Waiting for serial camera frame");
                    } else if (seq == lastSeq) {
                        pause(50);
                        continue;
                    }

                    outputStream.write(boundaryPrefix);
                    outputStream.write(frame);
                    outputStream.write(boundaryEnd);
                    outputStream.flush();
                    lastSeq = seq;
                    pause(frame == latestTankFrame.get() ? 20 : 500);
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } catch (Exception ignored) {
                // client disconnect
            }
        };
        return ResponseEntity.ok().contentType(MJPEG).body(body);
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

    private static boolean isJpeg(byte[] frame) {
        int len = frame.length;
        return (frame[0] & 0xFF) == 0xFF
                && (frame[1] & 0xFF) == 0xD8
                && (frame[len - 2] & 0xFF) == 0xFF
                && (frame[len - 1] & 0xFF) == 0xD9;
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
