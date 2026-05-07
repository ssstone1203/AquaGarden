package com.aquagarden.web;

import com.aquagarden.service.AquaBridgeService;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.server.ResponseStatusException;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;

import javax.imageio.ImageIO;
import java.awt.Color;
import java.awt.Font;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.Map;
import java.util.Set;

@RestController
public class AquaController {

    private static final Set<String> TASKS = Set.of("feed", "loosen", "prune", "stop");
    private static final Set<String> VIDEO_MODES = Set.of("rgb", "depth");
    private static final String BOUNDARY = "frame";
    private static final MediaType MJPEG = MediaType.parseMediaType("multipart/x-mixed-replace; boundary=" + BOUNDARY);

    private final AquaBridgeService bridgeService;

    public AquaController(AquaBridgeService bridgeService) {
        this.bridgeService = bridgeService;
    }

    @GetMapping("/api/aqua/status")
    public Map<String, Object> status() {
        return bridgeService.status();
    }

    @PostMapping("/api/aqua/tasks/{task}")
    public ResponseEntity<Object> task(@PathVariable String task) throws Exception {
        if (!TASKS.contains(task)) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "unknown task: " + task);
        }
        AquaBridgeService.BridgeResponse response = bridgeService.task(task);
        return bridgeResponse(response);
    }

    @PostMapping("/api/aqua/arm/home")
    public ResponseEntity<Object> armHome() throws Exception {
        AquaBridgeService.BridgeResponse response = bridgeService.armHome();
        return bridgeResponse(response);
    }

    @PostMapping("/api/aqua/arm/pose")
    public ResponseEntity<Object> armPose(@RequestBody Map<String, Object> body) throws Exception {
        AquaBridgeService.BridgeResponse response = bridgeService.armPose(body);
        return bridgeResponse(response);
    }

    @PostMapping("/api/aqua/arm/gripper")
    public ResponseEntity<Object> armGripper(@RequestBody Map<String, Object> body) throws Exception {
        AquaBridgeService.BridgeResponse response = bridgeService.armGripper(body);
        return bridgeResponse(response);
    }

    @PostMapping("/api/aqua/rail/position")
    public ResponseEntity<Object> moveRail(@RequestBody Map<String, Object> body) throws Exception {
        int position = parsePosition(body.get("position"));
        AquaBridgeService.BridgeResponse response = bridgeService.railPosition(position);
        return bridgeResponse(response);
    }

    @PostMapping("/api/aqua/rail/move")
    public ResponseEntity<Object> moveRailCompat(@RequestBody Map<String, Object> body) throws Exception {
        return moveRail(body);
    }

    @PostMapping("/api/aqua/pump/start")
    public ResponseEntity<Object> pumpStart(@RequestBody Map<String, Object> body) throws Exception {
        int pwm = parsePwm(body.get("pwm"));
        AquaBridgeService.BridgeResponse response = bridgeService.pumpStart(pwm);
        return bridgeResponse(response);
    }

    @PostMapping("/api/aqua/pump/pwm")
    public ResponseEntity<Object> pumpPwm(@RequestBody Map<String, Object> body) throws Exception {
        int pwm = parsePwm(body.get("pwm"));
        AquaBridgeService.BridgeResponse response = bridgeService.pumpPwm(pwm);
        return bridgeResponse(response);
    }

    @PostMapping("/api/aqua/pump/auto")
    public ResponseEntity<Object> pumpAuto() throws Exception {
        AquaBridgeService.BridgeResponse response = bridgeService.pumpAuto();
        return bridgeResponse(response);
    }

    @PostMapping("/api/aqua/pump/manual")
    public ResponseEntity<Object> pumpManual(@RequestBody Map<String, Object> body) throws Exception {
        int on = parseOn(body.get("on"));
        int pwm = parsePwm(body.get("pwm"));
        AquaBridgeService.BridgeResponse response = bridgeService.pumpManual(on, pwm);
        return bridgeResponse(response);
    }

    @PostMapping("/api/aqua/pump/stop")
    public ResponseEntity<Object> pumpStop() throws Exception {
        AquaBridgeService.BridgeResponse response = bridgeService.pumpManual(0, 0);
        return bridgeResponse(response);
    }

    @GetMapping(value = "/api/aqua/video/{mode}", produces = "multipart/x-mixed-replace; boundary=" + BOUNDARY)
    public ResponseEntity<StreamingResponseBody> video(@PathVariable String mode) {
        if (!VIDEO_MODES.contains(mode)) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "unknown video mode: " + mode);
        }
        StreamingResponseBody body = outputStream -> {
            try {
                bridgeService.streamVideo(mode, outputStream);
            } catch (IOException ignored) {
                writeFallbackVideo(mode, outputStream);
            }
        };
        return ResponseEntity.ok().contentType(MJPEG).body(body);
    }

    private ResponseEntity<Object> bridgeResponse(AquaBridgeService.BridgeResponse response) throws Exception {
        Object body = bridgeService.parseMap(response.body());
        HttpStatus status = HttpStatus.resolve(response.statusCode());
        if (status == null) {
            status = HttpStatus.BAD_GATEWAY;
        }
        return ResponseEntity.status(status).body(body);
    }

    private static int parsePosition(Object value) {
        int position;
        if (value instanceof Number n) {
            position = n.intValue();
        } else {
            try {
                position = Integer.parseInt(String.valueOf(value));
            } catch (NumberFormatException e) {
                throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "position must be 0..4000");
            }
        }
        if (position < 0 || position > 4000) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "position must be 0..4000");
        }
        return position;
    }

    private static int parsePwm(Object value) {
        int pwm;
        if (value instanceof Number n) {
            pwm = n.intValue();
        } else {
            try {
                pwm = Integer.parseInt(String.valueOf(value));
            } catch (NumberFormatException e) {
                throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "pwm must be 0..100");
            }
        }
        if (pwm < 0 || pwm > 100) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "pwm must be 0..100");
        }
        return pwm;
    }

    private static int parseOn(Object value) {
        int on;
        if (value instanceof Number n) {
            on = n.intValue();
        } else {
            try {
                on = Integer.parseInt(String.valueOf(value));
            } catch (NumberFormatException e) {
                throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "on must be 0 or 1");
            }
        }
        if (on != 0 && on != 1) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "on must be 0 or 1");
        }
        return on;
    }

    private static void writeFallbackVideo(String mode, java.io.OutputStream outputStream) throws IOException {
        byte[] boundaryPrefix = ("--" + BOUNDARY + "\r\nContent-Type: image/jpeg\r\n\r\n").getBytes(StandardCharsets.UTF_8);
        byte[] boundaryEnd = "\r\n".getBytes(StandardCharsets.UTF_8);
        while (!Thread.currentThread().isInterrupted()) {
            try {
                outputStream.write(boundaryPrefix);
                outputStream.write(generatedJpeg(mode));
                outputStream.write(boundaryEnd);
                outputStream.flush();
                pause(1000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } catch (Exception e) {
                throw new IOException("fallback video failed", e);
            }
        }
    }

    private static byte[] generatedJpeg(String mode) throws Exception {
        BufferedImage img = new BufferedImage(640, 400, BufferedImage.TYPE_INT_RGB);
        Graphics2D g = img.createGraphics();
        g.setColor(new Color(15, 23, 42));
        g.fillRect(0, 0, 640, 400);
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setColor(Color.WHITE);
        g.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 22));
        g.drawString("Aqua Bridge video unavailable", 28, 52);
        g.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 18));
        g.setColor(new Color(165, 243, 252));
        g.drawString(("depth".equals(mode) ? "Depth" : "RGB") + " stream proxy", 28, 86);
        g.setColor(new Color(148, 163, 184));
        String time = LocalTime.now().format(DateTimeFormatter.ofPattern("HH:mm:ss"));
        g.drawString("Waiting for Raspberry Pi Bridge · " + time, 28, 120);
        g.dispose();

        ByteArrayOutputStream jpg = new ByteArrayOutputStream();
        ImageIO.write(img, "jpg", jpg);
        return jpg.toByteArray();
    }

    private static void pause(long millis) throws InterruptedException {
        Thread.sleep(millis);
    }
}
