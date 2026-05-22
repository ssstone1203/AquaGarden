package com.aquagarden.service;

import com.aquagarden.dto.SensorSnapshot;
import com.aquagarden.entity.SensorReading;
import com.aquagarden.repo.SensorReadingRepository;
import com.aquagarden.websocket.LogWebSocketHandler;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fazecast.jSerialComm.SerialPort;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.Base64;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

@Service
public class HardwareSerialService {
    private static final Logger log = LoggerFactory.getLogger(HardwareSerialService.class);
    private static final int SYNC0 = 0x55;
    private static final int SYNC1 = 0xAA;
    private static final int HEADER_LEN = 6;
    private static final int CRC_LEN = 2;
    private static final int EXPECTED_PAYLOAD_LEN = 30;
    private static final int JPEG_SOI0 = 0xFF;
    private static final int JPEG_SOI1 = 0xD8;
    private static final int JPEG_EOI0 = 0xFF;
    private static final int JPEG_EOI1 = 0xD9;

    private final SystemStateService systemStateService;
    private final SensorReadingRepository readingRepo;
    private final SensorReadingRetentionService retentionService;
    private final TankVideoFrameService tankVideoFrameService;
    private final LogWebSocketHandler wsHandler;
    private final ObjectMapper objectMapper;
    private final boolean enabled;
    private final String portName;
    private final int baud;
    private final int maxJpegBytes;
    private final long persistIntervalMs;
    private final long reconnectDelayMs;
    private final Object serialWriteLock = new Object();

    private volatile boolean running;
    private volatile SerialPort serialPort;
    private volatile String lastError = "";
    private volatile boolean pumpManualOn;
    private volatile int pumpPwmUi;
    private volatile long startedAt = System.currentTimeMillis();
    private long lastPersistAt;
    private Thread worker;

    public HardwareSerialService(
            SystemStateService systemStateService,
            SensorReadingRepository readingRepo,
            SensorReadingRetentionService retentionService,
            TankVideoFrameService tankVideoFrameService,
            LogWebSocketHandler wsHandler,
            ObjectMapper objectMapper,
            @Value("${aquagarden.hardware.serial.enabled:false}") boolean enabled,
            @Value("${aquagarden.hardware.serial.port:COM3}") String portName,
            @Value("${aquagarden.hardware.serial.baud:115200}") int baud,
            @Value("${aquagarden.hardware.serial.max-jpeg-bytes:524288}") int maxJpegBytes,
            @Value("${aquagarden.hardware.serial.persist-interval-ms:1000}") long persistIntervalMs,
            @Value("${aquagarden.hardware.serial.reconnect-delay-ms:3000}") long reconnectDelayMs) {
        this.systemStateService = systemStateService;
        this.readingRepo = readingRepo;
        this.retentionService = retentionService;
        this.tankVideoFrameService = tankVideoFrameService;
        this.wsHandler = wsHandler;
        this.objectMapper = objectMapper;
        this.enabled = enabled;
        this.portName = portName;
        this.baud = baud;
        this.maxJpegBytes = Math.max(64 * 1024, maxJpegBytes);
        this.persistIntervalMs = Math.max(250L, persistIntervalMs);
        this.reconnectDelayMs = Math.max(500L, reconnectDelayMs);
    }

    @PostConstruct
    public void start() {
        if (!enabled) {
            return;
        }
        running = true;
        worker = new Thread(this::runLoop, "aquagarden-hardware-serial");
        worker.setDaemon(true);
        worker.start();
    }

    @PreDestroy
    public void stop() {
        running = false;
        closePort();
        if (worker != null) {
            worker.interrupt();
        }
    }

    public boolean isEnabled() {
        return enabled;
    }

    public boolean isConnected() {
        SerialPort port = serialPort;
        return port != null && port.isOpen();
    }

    public Map<String, Object> pumpDebugStatus() {
        return Map.of(
                "enabled", enabled,
                "connected", isConnected(),
                "port", portName,
                "baud", baud,
                "protocols", "legacy-5AA5 + uart-55AA-38B",
                "manualOn", pumpManualOn,
                "pwm", pumpManualOn ? pumpPwmUi : null,
                "lastError", lastError == null || lastError.isBlank() ? null : lastError
        );
    }

    public Map<String, Object> status(Map<String, Object> cameraStatus) {
        return Map.of(
                "ok", isConnected(),
                "connected", isConnected(),
                "busy", false,
                "currentTask", "idle",
                "phase", enabled ? "spring-serial" : "disabled",
                "railPosition", null,
                "lastError", lastError == null || lastError.isBlank() ? null : lastError,
                "uptimeSec", (System.currentTimeMillis() - startedAt) / 1000,
                "camera", cameraStatus,
                "pump", Map.of(
                        "manualOn", pumpManualOn,
                        "pwm", pumpManualOn ? pumpPwmUi : null
                )
        );
    }

    public AquaBridgeService.BridgeResponse pumpStart(int pwm) {
        int ui = clamp(pwm, 0, 100);
        if (ui == 0) {
            return pumpStop();
        }
        CommandResult result = sendCommand("PUMP", "START", String.valueOf(ui));
        if (result.ok()) {
            pumpManualOn = true;
            pumpPwmUi = ui;
        }
        return pumpResponse(result, Map.of("pwmUi", ui));
    }

    public AquaBridgeService.BridgeResponse pumpPwm(int pwm) {
        int ui = clamp(pwm, 0, 100);
        if (ui == 0) {
            return pumpStop();
        }
        CommandResult result = sendCommand("PUMP", "PWM", String.valueOf(ui));
        if (result.ok()) {
            pumpManualOn = true;
            pumpPwmUi = ui;
        }
        return pumpResponse(result, Map.of("pwmUi", ui));
    }

    public AquaBridgeService.BridgeResponse pumpStop() {
        CommandResult result = sendCommand("PUMP", "STOP");
        if (result.ok()) {
            pumpManualOn = true;
            pumpPwmUi = 0;
        }
        return pumpResponse(result, Map.of("pwmUi", 0));
    }

    public AquaBridgeService.BridgeResponse pumpAuto() {
        CommandResult result = sendCommand("PUMP", "AUTO");
        if (result.ok()) {
            pumpManualOn = false;
        }
        return pumpResponse(result, Map.of());
    }

    public AquaBridgeService.BridgeResponse pumpManual(int on, int pwm) {
        if (on == 0) {
            return pumpAuto();
        }
        int ui = clamp(pwm, 0, 100);
        if (ui == 0) {
            return pumpStop();
        }
        CommandResult result = sendCommand("PUMP", "MANUAL", "1", String.valueOf(ui));
        if (result.ok()) {
            pumpManualOn = true;
            pumpPwmUi = ui;
        }
        return pumpResponse(result, Map.of("on", on, "pwmUi", ui));
    }

    public AquaBridgeService.BridgeResponse pumpPulse(int seconds, int pwm) {
        int duration = clamp(seconds, 1, 120);
        int ui = clamp(pwm, 1, 100);
        CommandResult startResult = sendCommand("PUMP", "START", String.valueOf(ui));
        if (!startResult.ok()) {
            return pumpResponse(startResult, Map.of("seconds", duration, "pwmUi", ui));
        }
        Thread pulse = new Thread(() -> {
            try {
                Thread.sleep(duration * 1000L);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } finally {
                sendCommand("PUMP", "STOP");
                pumpManualOn = true;
                pumpPwmUi = 0;
            }
        }, "aquagarden-pump-pulse");
        pulse.setDaemon(true);
        pulse.start();
        pumpManualOn = true;
        pumpPwmUi = ui;
        return pumpResponse(new CommandResult(true, "pulse started"), Map.of("seconds", duration, "pwmUi", ui));
    }

    private void runLoop() {
        startedAt = System.currentTimeMillis();
        while (running) {
            SerialPort port = SerialPort.getCommPort(portName);
            port.setBaudRate(baud);
            port.setNumDataBits(8);
            port.setNumStopBits(SerialPort.ONE_STOP_BIT);
            port.setParity(SerialPort.NO_PARITY);
            port.setComPortTimeouts(SerialPort.TIMEOUT_READ_BLOCKING, 1000, 0);
            if (!port.openPort()) {
                lastError = "cannot open serial port " + portName;
                sleep(reconnectDelayMs);
                continue;
            }
            serialPort = port;
            lastError = "";
            log.info("AquaGarden hardware serial opened: {} @ {}", portName, baud);
            try {
                readPackets(port);
            } catch (Exception e) {
                lastError = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
                log.warn("hardware serial loop stopped: {}", lastError);
            } finally {
                closePort();
            }
            sleep(reconnectDelayMs);
        }
    }

    private void readPackets(SerialPort port) throws Exception {
        ByteArrayOutputStream line = new ByteArrayOutputStream();
        ByteArrayOutputStream jpeg = null;
        boolean inJpeg = false;
        int prev = -1;
        while (running && port.isOpen()) {
            int b = port.getInputStream().read();
            if (b < 0) {
                continue;
            }
            b &= 0xFF;

            if (prev == SYNC0 && b == SYNC1) {
                byte[] packet = readBinaryPacket(port);
                if (packet != null) {
                    handleSensorPayload(packet);
                }
                prev = -1;
                line.reset();
                continue;
            }

            if (!inJpeg && prev == JPEG_SOI0 && b == JPEG_SOI1) {
                inJpeg = true;
                jpeg = new ByteArrayOutputStream();
                jpeg.write(JPEG_SOI0);
                jpeg.write(JPEG_SOI1);
                prev = b;
                line.reset();
                continue;
            }

            if (inJpeg) {
                jpeg.write(b);
                if (jpeg.size() > maxJpegBytes) {
                    inJpeg = false;
                    jpeg = null;
                } else if (prev == JPEG_EOI0 && b == JPEG_EOI1) {
                    tankVideoFrameService.updateFrame(jpeg.toByteArray());
                    inJpeg = false;
                    jpeg = null;
                }
                prev = b;
                continue;
            }

            if (b == '\n' || b == '\r') {
                if (line.size() > 0) {
                    handleTextLine(line.toByteArray());
                    line.reset();
                }
            } else if (b >= 0x20 && b <= 0x7E && line.size() < maxJpegBytes * 2) {
                line.write(b);
            } else if (line.size() > maxJpegBytes * 2) {
                line.reset();
            }
            prev = b;
        }
    }

    private byte[] readBinaryPacket(SerialPort port) throws Exception {
        byte[] rest = port.getInputStream().readNBytes(HEADER_LEN - 2);
        if (rest.length != HEADER_LEN - 2) {
            return null;
        }
        int payloadLen = (rest[2] & 0xFF) | ((rest[3] & 0xFF) << 8);
        if (payloadLen <= 0 || payloadLen > 512) {
            return null;
        }
        byte[] body = port.getInputStream().readNBytes(payloadLen + CRC_LEN);
        if (body.length != payloadLen + CRC_LEN) {
            return null;
        }
        byte[] frame = new byte[HEADER_LEN + payloadLen + CRC_LEN];
        frame[0] = (byte) SYNC0;
        frame[1] = (byte) SYNC1;
        System.arraycopy(rest, 0, frame, 2, rest.length);
        System.arraycopy(body, 0, frame, HEADER_LEN, body.length);
        int got = ((frame[frame.length - 1] & 0xFF) << 8) | (frame[frame.length - 2] & 0xFF);
        int calc = crc16Modbus(frame, frame.length - CRC_LEN);
        if (got != calc) {
            return null;
        }
        byte[] payload = new byte[payloadLen];
        System.arraycopy(frame, HEADER_LEN, payload, 0, payloadLen);
        return payload;
    }

    private void handleSensorPayload(byte[] payload) {
        if (payload.length < EXPECTED_PAYLOAD_LEN) {
            return;
        }
        ByteBuffer buf = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);
        buf.getInt();
        double airTemp = round(buf.getShort() / 10.0, 1);
        double airHumidity = round(buf.getShort() / 10.0, 1);
        double waterTemp = round(buf.getShort() / 10.0, 1);
        double soilMoisture = buf.get() & 0xFF;
        double wqi = buf.get() & 0xFF;
        int pumpPct = buf.get() & 0xFF;
        pumpPwmUi = clamp(pumpPct, 0, 100);
        SensorSnapshot snapshot = new SensorSnapshot(waterTemp, airTemp, airHumidity, wqi, soilMoisture);
        publishSensorSnapshot(snapshot);
    }

    private void handleTextLine(byte[] raw) {
        String text = new String(raw, StandardCharsets.UTF_8).trim();
        if (text.isBlank()) {
            return;
        }
        Optional<byte[]> jpeg = decodeTextJpeg(text);
        if (jpeg.isPresent()) {
            tankVideoFrameService.updateFrame(jpeg.get());
            return;
        }
        Map<String, Double> values = parseTextSensorLine(text);
        if (values.isEmpty()) {
            return;
        }
        SensorSnapshot current = systemStateService.readSensorsWithNoise();
        SensorSnapshot snapshot = new SensorSnapshot(
                values.getOrDefault("water_temp", current.waterTemp()),
                values.getOrDefault("air_temp", current.airTemp()),
                values.getOrDefault("air_humidity", current.airHumidity()),
                values.getOrDefault("wqi", current.wqi()),
                values.getOrDefault("soil_moisture", current.soilMoisture())
        );
        publishSensorSnapshot(snapshot);
    }

    private void publishSensorSnapshot(SensorSnapshot snapshot) {
        long now = System.currentTimeMillis();
        systemStateService.updateFromHardware(snapshot, now);
        wsHandler.broadcastSensorData(snapshot);
        if (now - lastPersistAt >= persistIntervalMs) {
            readingRepo.save(new SensorReading(
                    Instant.ofEpochMilli(now),
                    snapshot.waterTemp(),
                    snapshot.airTemp(),
                    snapshot.airHumidity(),
                    snapshot.wqi(),
                    snapshot.soilMoisture()
            ));
            retentionService.enforceMaxRecordWindow();
            lastPersistAt = now;
        }
    }

    private Optional<byte[]> decodeTextJpeg(String text) {
        String lower = text.toLowerCase(Locale.ROOT);
        String payload = text;
        if (lower.startsWith("data:image/jpeg;base64,")) {
            payload = text.substring("data:image/jpeg;base64,".length());
        } else if (lower.startsWith("jpg:") || lower.startsWith("jpeg:")) {
            payload = text.substring(text.indexOf(':') + 1).trim();
        } else if (lower.startsWith("jpg_hex:") || lower.startsWith("jpeg_hex:")) {
            return decodeHex(text.substring(text.indexOf(':') + 1).trim());
        } else {
            return Optional.empty();
        }
        try {
            byte[] frame = Base64.getDecoder().decode(payload);
            return isJpeg(frame) ? Optional.of(frame) : Optional.empty();
        } catch (IllegalArgumentException e) {
            return Optional.empty();
        }
    }

    private static Optional<byte[]> decodeHex(String payload) {
        String clean = payload.replaceAll("\\s+", "");
        if ((clean.length() & 1) == 1) {
            return Optional.empty();
        }
        byte[] out = new byte[clean.length() / 2];
        try {
            for (int i = 0; i < out.length; i++) {
                out[i] = (byte) Integer.parseInt(clean.substring(i * 2, i * 2 + 2), 16);
            }
            return isJpeg(out) ? Optional.of(out) : Optional.empty();
        } catch (NumberFormatException e) {
            return Optional.empty();
        }
    }

    private static Map<String, Double> parseTextSensorLine(String text) {
        Map<String, Double> values = new HashMap<>();
        putIfPresent(values, "water_temp", numberAfter(text, "water_temp", "waterTemp", "water", "水温"));
        putIfPresent(values, "air_temp", numberAfter(text, "air_temp", "airTemp", "空气温度", "气温"));
        putIfPresent(values, "air_humidity", numberAfter(text, "air_humidity", "airHumidity", "humidity", "湿度"));
        putIfPresent(values, "wqi", numberAfter(text, "wqi", "water_quality", "水质"));
        putIfPresent(values, "soil_moisture", numberAfter(text, "soil_moisture", "soilMoisture", "soil", "土壤湿度"));
        return values;
    }

    private static void putIfPresent(Map<String, Double> values, String key, Optional<Double> value) {
        value.ifPresent(v -> values.put(key, v));
    }

    private static Optional<Double> numberAfter(String text, String... keys) {
        for (String key : keys) {
            Pattern pattern = Pattern.compile(Pattern.quote(key) + "\\s*[:=：]?\\s*([-+]?\\d+(?:\\.\\d+)?)", Pattern.CASE_INSENSITIVE);
            Matcher matcher = pattern.matcher(text);
            if (matcher.find()) {
                return Optional.of(Double.parseDouble(matcher.group(1)));
            }
        }
        return Optional.empty();
    }

    private CommandResult sendCommand(String... parts) {
        SerialPort port = serialPort;
        if (port == null || !port.isOpen()) {
            lastError = "serial port is not open";
            return new CommandResult(false, lastError);
        }
        byte[][] frames = pumpCommandFrames(parts);
        if (frames.length == 0) {
            lastError = "unknown pump action";
            return new CommandResult(false, lastError);
        }
        try {
            synchronized (serialWriteLock) {
                for (byte[] data : frames) {
                    int written = port.writeBytes(data, data.length);
                    if (written != data.length) {
                        lastError = "serial write incomplete";
                        return new CommandResult(false, lastError);
                    }
                    sleep(20);
                }
            }
            lastError = "";
            return new CommandResult(true, "ok");
        } catch (Exception e) {
            lastError = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
            return new CommandResult(false, lastError);
        }
    }

    private static byte[][] pumpCommandFrames(String... parts) {
        if (parts == null || parts.length < 2) {
            return new byte[0][];
        }
        String action = parts[1].toUpperCase(Locale.ROOT);
        int power = 0;
        if (parts.length > 0) {
            try {
                power = clamp(Integer.parseInt(parts[parts.length - 1]), 0, 100);
            } catch (NumberFormatException ignored) {
                power = 0;
            }
        }

        byte legacyCmd = 0x01;
        byte[] legacyPayload = new byte[] { 1, (byte) power };
        int uartCmd = 0x03;
        int uartPower = power;

        if ("STOP".equals(action)) {
            legacyPayload = new byte[] { 1, 0 };
            uartCmd = 0x01;
            uartPower = 0;
        } else if ("AUTO".equals(action)) {
            legacyPayload = new byte[] { 0, 0 };
            uartCmd = 0x01;
            uartPower = 0;
        } else if ("START".equals(action)) {
            if (power <= 0) {
                power = 60;
            }
            legacyPayload = new byte[] { 1, (byte) power };
            uartCmd = 0x02;
            uartPower = power;
        } else if ("PWM".equals(action) || "MANUAL".equals(action)) {
            int enable = 1;
            if ("MANUAL".equals(action) && parts.length >= 4) {
                try {
                    enable = clamp(Integer.parseInt(parts[2]), 0, 1);
                } catch (NumberFormatException ignored) {
                    enable = 1;
                }
            }
            legacyPayload = new byte[] { (byte) enable, (byte) power };
            uartCmd = enable == 1 ? 0x03 : 0x01;
            uartPower = enable == 1 ? power : 0;
        } else {
            return new byte[0][];
        }

        return new byte[][] {
                packLegacyDownlink(legacyCmd, legacyPayload),
                packUartCommDownlink(uartCmd, uartPower)
        };
    }

    private static byte[] packLegacyDownlink(byte cmd, byte[] payload) {
        int length = 1 + payload.length;
        byte[] frame = new byte[4 + payload.length + 2];
        frame[0] = 0x5A;
        frame[1] = (byte) 0xA5;
        frame[2] = (byte) (length & 0xFF);
        frame[3] = cmd;
        System.arraycopy(payload, 0, frame, 4, payload.length);
        int crc = crc16Modbus(frame, frame.length - 2);
        frame[frame.length - 2] = (byte) (crc & 0xFF);
        frame[frame.length - 1] = (byte) ((crc >>> 8) & 0xFF);
        return frame;
    }

    private static byte[] packUartCommDownlink(int cmd, int power) {
        byte[] frame = new byte[38];
        frame[0] = 0x55;
        frame[1] = (byte) 0xAA;
        frame[2] = (byte) (cmd & 0xFF);
        frame[3] = (byte) (power & 0xFF);
        int crc = crc16Modbus(frame, frame.length - 2);
        frame[frame.length - 2] = (byte) (crc & 0xFF);
        frame[frame.length - 1] = (byte) ((crc >>> 8) & 0xFF);
        return frame;
    }

    private AquaBridgeService.BridgeResponse pumpResponse(CommandResult result, Map<String, Object> extra) {
        Map<String, Object> body = new HashMap<>();
        body.put("ok", result.ok());
        body.put("connected", isConnected());
        body.put("message", result.message());
        body.put("pump", Map.of(
                "manualOn", pumpManualOn,
                "pwm", pumpManualOn ? pumpPwmUi : null
        ));
        body.putAll(extra);
        return new AquaBridgeService.BridgeResponse(result.ok() ? 200 : 502, writeJson(body));
    }

    private void closePort() {
        SerialPort port = serialPort;
        serialPort = null;
        if (port != null && port.isOpen()) {
            port.closePort();
        }
    }

    private static int crc16Modbus(byte[] data, int len) {
        int crc = 0xFFFF;
        for (int i = 0; i < len; i++) {
            crc ^= data[i] & 0xFF;
            for (int bit = 0; bit < 8; bit++) {
                if ((crc & 1) != 0) {
                    crc = (crc >>> 1) ^ 0xA001;
                } else {
                    crc >>>= 1;
                }
            }
        }
        return crc & 0xFFFF;
    }

    private static boolean isJpeg(byte[] frame) {
        return frame != null
                && frame.length >= 4
                && (frame[0] & 0xFF) == JPEG_SOI0
                && (frame[1] & 0xFF) == JPEG_SOI1
                && (frame[frame.length - 2] & 0xFF) == JPEG_EOI0
                && (frame[frame.length - 1] & 0xFF) == JPEG_EOI1;
    }

    private static int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }

    private static double round(double value, int decimals) {
        double factor = Math.pow(10, decimals);
        return Math.round(value * factor) / factor;
    }

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    private String writeJson(Map<String, Object> body) {
        try {
            return objectMapper.writeValueAsString(body);
        } catch (JsonProcessingException e) {
            return "{\"ok\":false,\"message\":\"json serialization failed\"}";
        }
    }

    private record CommandResult(boolean ok, String message) {
    }
}
