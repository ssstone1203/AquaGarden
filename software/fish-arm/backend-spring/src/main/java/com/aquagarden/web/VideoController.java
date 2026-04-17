package com.aquagarden.web;

import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;

import javax.imageio.ImageIO;
import java.awt.*;
import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.Random;

@RestController
public class VideoController {

    private static final String BOUNDARY = "frame";
    private static final MediaType MJPEG = MediaType.parseMediaType("multipart/x-mixed-replace; boundary=" + BOUNDARY);

    @GetMapping(value = "/api/video/robot", produces = "multipart/x-mixed-replace; boundary=" + BOUNDARY)
    public ResponseEntity<StreamingResponseBody> robot() {
        return stream("机械臂", "Camera Feed");
    }

    @GetMapping(value = "/api/video/tank", produces = "multipart/x-mixed-replace; boundary=" + BOUNDARY)
    public ResponseEntity<StreamingResponseBody> tank() {
        return stream("鱼缸", "Camera Feed");
    }

    private ResponseEntity<StreamingResponseBody> stream(String label, String subtitle) {
        StreamingResponseBody body = outputStream -> {
            Random rnd = new Random();
            byte[] boundaryPrefix = ("--" + BOUNDARY + "\r\nContent-Type: image/jpeg\r\n\r\n").getBytes(StandardCharsets.UTF_8);
            byte[] boundaryEnd = "\r\n".getBytes(StandardCharsets.UTF_8);
            try {
                while (!Thread.currentThread().isInterrupted()) {
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

                    outputStream.write(boundaryPrefix);
                    outputStream.write(jpg.toByteArray());
                    outputStream.write(boundaryEnd);
                    outputStream.flush();
                    Thread.sleep(100);
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } catch (Exception ignored) {
                // client disconnect
            }
        };
        return ResponseEntity.ok().contentType(MJPEG).body(body);
    }
}
