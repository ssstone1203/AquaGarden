package com.aquagarden.service;

import org.springframework.stereotype.Service;

import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

@Service
public class TankVideoFrameService {
    private static final int MAX_JPEG_BYTES = 1024 * 1024;

    private final AtomicReference<byte[]> latestFrame = new AtomicReference<>();
    private final AtomicLong latestFrameAt = new AtomicLong(0L);
    private final AtomicLong frameSeq = new AtomicLong(0L);

    public boolean updateFrame(byte[] frame) {
        if (frame == null || frame.length < 4 || frame.length > MAX_JPEG_BYTES || !isJpeg(frame)) {
            return false;
        }
        latestFrame.set(frame);
        latestFrameAt.set(System.currentTimeMillis());
        frameSeq.incrementAndGet();
        return true;
    }

    public byte[] latestFrame() {
        return latestFrame.get();
    }

    public long latestFrameAt() {
        return latestFrameAt.get();
    }

    public long frameSeq() {
        return frameSeq.get();
    }

    public Map<String, Object> status(int detectionCount) {
        byte[] frame = latestFrame.get();
        return Map.of(
                "hasFrame", frame != null,
                "seq", frameSeq.get(),
                "updatedAt", latestFrameAt.get(),
                "bytes", frame == null ? 0 : frame.length,
                "detectionCount", detectionCount
        );
    }

    private static boolean isJpeg(byte[] frame) {
        int len = frame.length;
        return (frame[0] & 0xFF) == 0xFF
                && (frame[1] & 0xFF) == 0xD8
                && (frame[len - 2] & 0xFF) == 0xFF
                && (frame[len - 1] & 0xFF) == 0xD9;
    }
}
