from __future__ import annotations

import base64
import re
import struct
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from typing import Iterable

from sqlalchemy.orm import Session

from app.core.config import settings
from app.db.session import SessionLocal
from app.models.sensor_reading import SensorReading
from app.schemas.common import SensorSnapshot
from app.services.logs import hub
from app.services.state import state
from app.services.video import tank_video

try:
    import serial
except ImportError:  # pragma: no cover - pyserial is optional unless hardware mode is enabled.
    serial = None


SYNC = b"\x55\xaa"
JPEG_SOI = b"\xff\xd8"
JPEG_EOI = b"\xff\xd9"
EXPECTED_PAYLOAD_LEN = 30


@dataclass
class SerialCommandResult:
    ok: bool
    message: str


class HardwareSerialService:
    def __init__(self) -> None:
        self.enabled = settings.hardware_serial_enabled
        self.port_name = settings.hardware_serial_port
        self.baud = settings.hardware_serial_baud
        self.max_jpeg_bytes = max(64 * 1024, settings.hardware_serial_max_jpeg_bytes)
        self.persist_interval_ms = max(250, settings.hardware_serial_persist_interval_ms)
        self.reconnect_delay_ms = max(500, settings.hardware_serial_reconnect_delay_ms)
        self._serial = None
        self._running = False
        self._thread: threading.Thread | None = None
        self._write_lock = threading.Lock()
        self._last_error: str | None = None
        self._started_at = int(time.time() * 1000)
        self._last_persist_at = 0
        self.pump_manual_on = False
        self.pump_pwm_ui: int | None = None

    def start(self) -> None:
        if not self.enabled or self._running:
            return
        if serial is None:
            self._last_error = "pyserial is not installed"
            return
        self._running = True
        self._thread = threading.Thread(target=self._run_loop, name="aquagarden-hardware-serial", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._running = False
        self._close()

    def is_connected(self) -> bool:
        port = self._serial
        return bool(port and getattr(port, "is_open", False))

    def status(self, camera_status: dict | None = None) -> dict:
        return {
            "ok": self.is_connected(),
            "connected": self.is_connected(),
            "busy": False,
            "currentTask": "idle",
            "phase": "fastapi-serial" if self.enabled else "disabled",
            "railPosition": None,
            "lastError": self._last_error,
            "uptimeSec": (int(time.time() * 1000) - self._started_at) // 1000,
            "camera": camera_status or {"hasRgb": bool(settings.camera_rgb_url), "hasDepth": bool(settings.camera_depth_url)},
            "pump": {"manualOn": self.pump_manual_on, "pwm": self.pump_pwm_ui if self.pump_manual_on else None},
        }

    def pump_debug_status(self) -> dict:
        return {
            "enabled": self.enabled,
            "connected": self.is_connected(),
            "port": self.port_name,
            "baud": self.baud,
            "protocols": "legacy-5AA5 + uart-55AA-38B",
            "manualOn": self.pump_manual_on,
            "pwm": self.pump_pwm_ui if self.pump_manual_on else None,
            "lastError": self._last_error,
        }

    def pump_start(self, pwm: int) -> tuple[int, dict]:
        ui = _clamp(pwm, 0, 100)
        if ui == 0:
            return self.pump_stop()
        result = self._send_command("START", ui)
        if result.ok:
            self.pump_manual_on = True
            self.pump_pwm_ui = ui
        return self._pump_response(result, {"pwmUi": ui})

    def pump_pwm(self, pwm: int) -> tuple[int, dict]:
        ui = _clamp(pwm, 0, 100)
        if ui == 0:
            return self.pump_stop()
        result = self._send_command("PWM", ui)
        if result.ok:
            self.pump_manual_on = True
            self.pump_pwm_ui = ui
        return self._pump_response(result, {"pwmUi": ui})

    def pump_stop(self) -> tuple[int, dict]:
        result = self._send_command("STOP", 0)
        if result.ok:
            self.pump_manual_on = True
            self.pump_pwm_ui = 0
        return self._pump_response(result, {"pwmUi": 0})

    def pump_auto(self) -> tuple[int, dict]:
        result = self._send_command("AUTO", 0)
        if result.ok:
            self.pump_manual_on = False
            self.pump_pwm_ui = None
        return self._pump_response(result, {})

    def pump_manual(self, on: int, pwm: int) -> tuple[int, dict]:
        if on == 0:
            return self.pump_auto()
        ui = _clamp(pwm, 0, 100)
        if ui == 0:
            return self.pump_stop()
        result = self._send_command("MANUAL", ui, enable=1)
        if result.ok:
            self.pump_manual_on = True
            self.pump_pwm_ui = ui
        return self._pump_response(result, {"on": on, "pwmUi": ui})

    def pump_pulse(self, seconds: int, pwm: int) -> tuple[int, dict]:
        duration = _clamp(seconds, 1, 120)
        ui = _clamp(pwm, 1, 100)
        result = self._send_command("START", ui)
        if not result.ok:
            return self._pump_response(result, {"seconds": duration, "pwmUi": ui})
        self.pump_manual_on = True
        self.pump_pwm_ui = ui
        threading.Thread(target=self._pulse_stop_later, args=(duration,), name="aquagarden-pump-pulse", daemon=True).start()
        return self._pump_response(SerialCommandResult(True, "pulse started"), {"seconds": duration, "pwmUi": ui})

    def _pulse_stop_later(self, seconds: int) -> None:
        time.sleep(seconds)
        self._send_command("STOP", 0)
        self.pump_manual_on = True
        self.pump_pwm_ui = 0

    def _pump_response(self, result: SerialCommandResult, extra: dict) -> tuple[int, dict]:
        body = {
            "ok": result.ok,
            "connected": self.is_connected(),
            "message": result.message,
            "pump": {"manualOn": self.pump_manual_on, "pwm": self.pump_pwm_ui if self.pump_manual_on else None},
            **extra,
        }
        return (200 if result.ok else 502), body

    def _run_loop(self) -> None:
        self._started_at = int(time.time() * 1000)
        while self._running:
            try:
                self._serial = serial.Serial(self.port_name, self.baud, timeout=1)
                self._last_error = None
                self._read_loop()
            except Exception as exc:
                self._last_error = str(exc) or exc.__class__.__name__
            finally:
                self._close()
            time.sleep(self.reconnect_delay_ms / 1000)

    def _read_loop(self) -> None:
        assert self._serial is not None
        line = bytearray()
        jpeg: bytearray | None = None
        prev = -1
        while self._running and self.is_connected():
            raw = self._serial.read(1)
            if not raw:
                continue
            b = raw[0]
            if prev == SYNC[0] and b == SYNC[1]:
                payload = self._read_binary_packet()
                if payload is not None:
                    self._handle_sensor_payload(payload)
                prev = -1
                line.clear()
                continue
            if jpeg is None and prev == JPEG_SOI[0] and b == JPEG_SOI[1]:
                jpeg = bytearray(JPEG_SOI)
                prev = b
                line.clear()
                continue
            if jpeg is not None:
                jpeg.append(b)
                if len(jpeg) > self.max_jpeg_bytes:
                    jpeg = None
                elif prev == JPEG_EOI[0] and b == JPEG_EOI[1]:
                    tank_video.update_frame(bytes(jpeg))
                    jpeg = None
                prev = b
                continue
            if b in (10, 13):
                if line:
                    self._handle_text_line(bytes(line))
                    line.clear()
            elif 0x20 <= b <= 0x7E and len(line) < self.max_jpeg_bytes * 2:
                line.append(b)
            else:
                if len(line) >= self.max_jpeg_bytes * 2:
                    line.clear()
            prev = b

    def _read_binary_packet(self) -> bytes | None:
        assert self._serial is not None
        rest = self._serial.read(4)
        if len(rest) != 4:
            return None
        payload_len = rest[2] | (rest[3] << 8)
        if payload_len <= 0 or payload_len > 512:
            return None
        body = self._serial.read(payload_len + 2)
        if len(body) != payload_len + 2:
            return None
        frame = SYNC + rest + body
        got = frame[-2] | (frame[-1] << 8)
        if got != crc16_modbus(frame[:-2]):
            return None
        return bytes(frame[6:-2])

    def _handle_sensor_payload(self, payload: bytes) -> None:
        if len(payload) < EXPECTED_PAYLOAD_LEN:
            return
        _, air_temp_i, air_humidity_i, water_temp_i, soil, wqi, pump_pct = struct.unpack_from("<ihhhBBB", payload, 0)
        self.pump_pwm_ui = _clamp(pump_pct, 0, 100)
        snapshot = SensorSnapshot(
            water_temp=round(water_temp_i / 10.0, 1),
            air_temp=round(air_temp_i / 10.0, 1),
            air_humidity=round(air_humidity_i / 10.0, 1),
            wqi=float(wqi),
            soil_moisture=float(soil),
        )
        self._publish_snapshot(snapshot)

    def _handle_text_line(self, raw: bytes) -> None:
        text = raw.decode("utf-8", errors="ignore").strip()
        if not text:
            return
        frame = _decode_text_jpeg(text)
        if frame:
            tank_video.update_frame(frame)
            return
        values = _parse_text_sensor_line(text)
        if not values:
            return
        current = state.read_sensors()
        snapshot = SensorSnapshot(
            water_temp=values.get("water_temp", current.water_temp),
            air_temp=values.get("air_temp", current.air_temp),
            air_humidity=values.get("air_humidity", current.air_humidity),
            wqi=values.get("wqi", current.wqi),
            soil_moisture=values.get("soil_moisture", current.soil_moisture),
        )
        self._publish_snapshot(snapshot)

    def _publish_snapshot(self, snapshot: SensorSnapshot) -> None:
        ts = int(time.time() * 1000)
        state.update_sensor(snapshot, ts)
        try:
            import asyncio

            asyncio.run(hub.broadcast({"type": "sensor", "data": snapshot.model_dump()}))
        except Exception:
            pass
        if ts - self._last_persist_at >= self.persist_interval_ms:
            db = SessionLocal()
            try:
                _save_reading(db, snapshot, datetime.fromtimestamp(ts / 1000))
                self._last_persist_at = ts
            finally:
                db.close()

    def _send_command(self, action: str, power: int, enable: int = 1) -> SerialCommandResult:
        port = self._serial
        if port is None or not self.is_connected():
            self._last_error = "serial port is not open"
            return SerialCommandResult(False, self._last_error)
        frames = list(pump_command_frames(action, power, enable))
        if not frames:
            self._last_error = "unknown pump action"
            return SerialCommandResult(False, self._last_error)
        try:
            with self._write_lock:
                for frame in frames:
                    port.write(frame)
                    port.flush()
                    time.sleep(0.02)
            self._last_error = None
            return SerialCommandResult(True, "ok")
        except Exception as exc:
            self._last_error = str(exc) or exc.__class__.__name__
            return SerialCommandResult(False, self._last_error)

    def _close(self) -> None:
        port = self._serial
        self._serial = None
        if port is not None:
            try:
                port.close()
            except Exception:
                pass


def _save_reading(db: Session, snapshot: SensorSnapshot, recorded_at: datetime) -> None:
    db.add(SensorReading(recorded_at=int(recorded_at.timestamp() * 1000), **snapshot.model_dump()))
    db.commit()


def pump_command_frames(action: str, power: int, enable: int = 1) -> Iterable[bytes]:
    action = action.upper()
    power = _clamp(power, 0, 100)
    legacy_cmd = 0x01
    legacy_payload = bytes([1, power])
    uart_cmd = 0x03
    uart_power = power

    if action == "STOP":
        legacy_payload = b"\x01\x00"
        uart_cmd, uart_power = 0x01, 0
    elif action == "AUTO":
        legacy_payload = b"\x00\x00"
        uart_cmd, uart_power = 0x01, 0
    elif action == "START":
        power = power or 60
        legacy_payload = bytes([1, power])
        uart_cmd, uart_power = 0x02, power
    elif action in {"PWM", "MANUAL"}:
        enable = _clamp(enable, 0, 1)
        legacy_payload = bytes([enable, power])
        uart_cmd, uart_power = (0x03, power) if enable else (0x01, 0)
    else:
        return []
    return [pack_legacy_downlink(legacy_cmd, legacy_payload), pack_uart_comm_downlink(uart_cmd, uart_power)]


def pack_legacy_downlink(cmd: int, payload: bytes) -> bytes:
    frame = bytearray([0x5A, 0xA5, (1 + len(payload)) & 0xFF, cmd & 0xFF])
    frame.extend(payload)
    crc = crc16_modbus(frame)
    frame.extend([crc & 0xFF, (crc >> 8) & 0xFF])
    return bytes(frame)


def pack_uart_comm_downlink(cmd: int, power: int) -> bytes:
    frame = bytearray(38)
    frame[0] = 0x55
    frame[1] = 0xAA
    frame[2] = cmd & 0xFF
    frame[3] = power & 0xFF
    crc = crc16_modbus(frame[:-2])
    frame[-2] = crc & 0xFF
    frame[-1] = (crc >> 8) & 0xFF
    return bytes(frame)


def crc16_modbus(data: bytes | bytearray) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc >> 1) ^ 0xA001) if (crc & 1) else (crc >> 1)
    return crc & 0xFFFF


def _decode_text_jpeg(text: str) -> bytes | None:
    lower = text.lower()
    payload = text
    if lower.startswith("data:image/jpeg;base64,"):
        payload = text[len("data:image/jpeg;base64,") :]
    elif lower.startswith(("jpg:", "jpeg:")):
        payload = text.split(":", 1)[1].strip()
    elif lower.startswith(("jpg_hex:", "jpeg_hex:")):
        return _decode_hex_jpeg(text.split(":", 1)[1].strip())
    else:
        return None
    try:
        frame = base64.b64decode(payload)
        return frame if _is_jpeg(frame) else None
    except Exception:
        return None


def _decode_hex_jpeg(payload: str) -> bytes | None:
    clean = re.sub(r"\s+", "", payload)
    if len(clean) % 2:
        return None
    try:
        frame = bytes.fromhex(clean)
        return frame if _is_jpeg(frame) else None
    except ValueError:
        return None


def _parse_text_sensor_line(text: str) -> dict[str, float]:
    return {
        key: value
        for key, value in {
            "water_temp": _number_after(text, "water_temp", "waterTemp", "water", "水温"),
            "air_temp": _number_after(text, "air_temp", "airTemp", "空气温度", "气温"),
            "air_humidity": _number_after(text, "air_humidity", "airHumidity", "humidity", "湿度"),
            "wqi": _number_after(text, "wqi", "water_quality", "水质"),
            "soil_moisture": _number_after(text, "soil_moisture", "soilMoisture", "soil", "土壤湿度"),
        }.items()
        if value is not None
    }


def _number_after(text: str, *keys: str) -> float | None:
    for key in keys:
        match = re.search(re.escape(key) + r"\s*[:=：]?\s*([-+]?\d+(?:\.\d+)?)", text, re.IGNORECASE)
        if match:
            return float(match.group(1))
    return None


def _is_jpeg(frame: bytes) -> bool:
    return len(frame) >= 4 and frame[:2] == JPEG_SOI and frame[-2:] == JPEG_EOI


def _clamp(value: int, min_value: int, max_value: int) -> int:
    return max(min_value, min(max_value, int(value)))


hardware_serial = HardwareSerialService()
