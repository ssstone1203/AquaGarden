"""
serial_bridge.py — MCU UART → Spring Boot 桥接脚本
================================================
读取 hardware/demo_wyr 固件（Communicate_Task_entry.c）发出的二进制上行帧，
解析后 POST 到 /api/sensors/ingest，后端前端轮询 /api/sensors 即可得到真实数据。
鱼缸 USB 摄像头默认使用索引 1，本脚本会用 OpenCV 抓帧并 POST 到
/api/video/tank/ingest，前端摄像头页通过 /api/video/tank 展示实时画面。
独立的 YOLO 等推理进程可将检测框 POST 至 /api/video/tank/detections（JSON），
后端在 MJPEG 流上叠加矩形框。
如果同一串口也混入 JPEG 数据，本脚本仍会按 FF D8 ... FF D9 提取图片帧上传。

依赖：
    pip install pyserial requests opencv-python ultralytics

用法：
    python serial_bridge.py --port COM3 --backend http://localhost:8090
    python serial_bridge.py --port /dev/ttyUSB0 --backend http://127.0.0.1:8090
    python serial_bridge.py --port COM3 --backend http://localhost:8090 --max-jpeg-bytes 1048576
    python serial_bridge.py --port COM3 --backend http://localhost:8090 --tank-camera-index 1
    python serial_bridge.py --port COM3 --backend http://localhost:8090 --no-yolo
    python serial_bridge.py --port COM3 --backend http://localhost:8090 --yolo-weights E:/path/best.pt --yolo-device cpu

MCU 上行帧格式（每 250 ms 一帧，Modbus CRC-16）：
  [0]    0x55          同步头 0
  [1]    0xAA          同步头 1
  [2]    0x01          版本号
  [3]    seq           帧序号（循环）
  [4:5]  payload_len   小端 uint16，当前固件固定 30 字节
  [6:35] payload       见下方 PAYLOAD 字段说明
  [36:37] crc16        Modbus CRC-16（覆盖 [0:35]）

PAYLOAD 字段（30 字节，均为小端）：
  偏移  长度  类型      固件变量                    说明
   0    4   uint32   g_jscope_time_ms           系统运行毫秒数
   4    2   int16    g_sht30_temperature_c × 10  SHT30 空气温度 (°C×10, 范围 -40~125°C)
   6    2   int16    g_sht30_humidity_rh × 10    SHT30 空气湿度 (%RH×10, 0~100%)
   8    2   int16    g_uwt_temperature_c × 10   DS18B20 水温 (°C×10, 范围 -55~125°C)
  10    1   uint8    g_soil_moisture_percent    土壤湿度 ADC 0-100 %
  11    1   uint8    wqs_info_wqi               WQM11S 水质综合指数 0-100
  12    1   uint8    g_pump_actual_power_percent 泵占空比 %
  13    1   uint8    g_control_need_watering    是否触发浇水
  14    2   uint16   pressure_kg[0] × 100       压力传感器0 (max 5.0 kg)
  16    2   uint16   pressure_kg[1] × 100       压力传感器1
  18    2   uint16   pressure_kg[2] × 100       压力传感器2
  20    4   uint32   g_alarm_flags              报警标志位
  24    2   uint16   air_retry_count
  26    2   uint16   wqs_retry_count
  28    2   uint16   uwt_retry_count

字段直接映射到前端（5 个真实传感器指标）：
  water_temp    ← g_uwt_temperature_c / 10.0  DS18B20 水温 (°C)
  air_temp      ← g_sht30_temperature_c / 10.0  SHT30 空气温度 (°C)
  air_humidity  ← g_sht30_humidity_rh / 10.0   SHT30 空气湿度 (%RH)
  wqi           ← wqs_info_wqi                  WQM11S 水质综合指数 (0-100)
  soil_moisture ← g_soil_moisture_percent        土壤湿度 (0-100 %)
"""

import argparse
import base64
import binascii
import logging
import struct
import threading
import time
from dataclasses import dataclass
from pathlib import Path

import requests
import serial

# 仓库根：software/fish-arm/serial_bridge.py → parents[2] == AquaGarden
_REPO_ROOT = Path(__file__).resolve().parents[2]
_DEFAULT_YOLO_WEIGHTS = _REPO_ROOT / "model" / "yolo_fish" / "runs" / "yolo11n_fish_new" / "weights" / "best.pt"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("bridge")

_yolo_predict_lock = threading.Lock()

SYNC0 = 0x55
SYNC1 = 0xAA
HEADER_LEN = 6   # sync0 + sync1 + version + seq + payload_len(2)
CRC_LEN = 2
EXPECTED_PAYLOAD_LEN = 30
PUSH_INTERVAL_SEC = 1.0
JPEG_SOI = b"\xff\xd8"
JPEG_EOI = b"\xff\xd9"
DEFAULT_MAX_JPEG_BYTES = 512 * 1024
BASE64_PREFIX = b"data:image/jpeg;base64,"
ASCII_LINE_LIMIT_FACTOR = 2
DEFAULT_TANK_CAMERA_INDEX = 0
DEFAULT_CAMERA_FPS = 10.0
JPEG_ENCODE_QUALITY = 80


@dataclass
class SerialPacket:
    kind: str
    data: bytes


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def parse_payload(payload: bytes) -> dict:
    """解析 30 字节 payload，返回物理量字典。"""
    if len(payload) < EXPECTED_PAYLOAD_LEN:
        raise ValueError(f"payload 长度不足：{len(payload)} < {EXPECTED_PAYLOAD_LEN}")

    (time_ms,) = struct.unpack_from("<I", payload, 0)
    (air_temp_x10,) = struct.unpack_from("<h", payload, 4)
    (air_humi_x10,) = struct.unpack_from("<h", payload, 6)
    (water_temp_x10,) = struct.unpack_from("<h", payload, 8)
    soil_pct = payload[10]
    wqi = payload[11]
    pump_pct = payload[12]
    need_water = payload[13]
    (p0_x100,) = struct.unpack_from("<H", payload, 14)
    (p1_x100,) = struct.unpack_from("<H", payload, 16)
    (p2_x100,) = struct.unpack_from("<H", payload, 18)
    (alarm_flags,) = struct.unpack_from("<I", payload, 20)

    air_temp   = round(air_temp_x10  / 10.0, 1)
    air_humi   = round(air_humi_x10  / 10.0, 1)
    water_temp = round(water_temp_x10 / 10.0, 1)
    pressure   = [round(p0_x100 / 100.0, 2), round(p1_x100 / 100.0, 2), round(p2_x100 / 100.0, 2)]

    return {
        "raw": {
            "time_ms":    time_ms,
            "air_temp":   air_temp,
            "air_humidity": air_humi,
            "water_temp": water_temp,
            "soil_pct":   soil_pct,
            "wqi":        wqi,
            "pump_pct":   pump_pct,
            "need_water": need_water,
            "pressure":   pressure,
            "alarm_flags": alarm_flags,
        },
        # 与后端 /api/sensors/ingest 及 SensorSnapshot 字段一一对应
        "ingest": {
            "water_temp":    water_temp,
            "air_temp":      air_temp,
            "air_humidity":  air_humi,
            "wqi":           float(wqi),
            "soil_moisture": float(soil_pct),
        },
    }


def decode_text_jpeg(line: bytes) -> bytes | None:
    """兼容串口输出 base64/hex 文本图片帧。"""
    text = line.strip()
    if not text:
        return None

    if text.startswith(BASE64_PREFIX):
        text = text[len(BASE64_PREFIX):]
    else:
        start = text.find(b"/9j/")
        if start >= 0:
            text = text[start:]

    if text.startswith(b"/9j/"):
        allowed = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="
        payload = bytes(ch for ch in text if ch in allowed)
        try:
            frame = base64.b64decode(payload, validate=False)
        except (binascii.Error, ValueError):
            return None
        return frame if is_jpeg(frame) else None

    if text[:4].upper() == b"FFD8":
        hex_payload = bytes(ch for ch in text if chr(ch).isalnum())
        try:
            frame = bytes.fromhex(hex_payload.decode("ascii"))
        except ValueError:
            return None
        return frame if is_jpeg(frame) else None

    return None


def is_jpeg(frame: bytes) -> bool:
    return (
        len(frame) >= 4
        and frame[0:2] == JPEG_SOI
        and frame[-2:] == JPEG_EOI
    )


def read_packet(ser: serial.Serial, max_jpeg_bytes: int) -> SerialPacket | None:
    """从混合串口流读取传感器帧或 JPEG 帧。"""
    b0 = ser.read(1)
    if not b0:
        return None

    # 传感器帧：55 AA ...
    if b0[0] == SYNC0:
        b1 = ser.read(1)
        if not b1:
            return None
        if b1[0] != SYNC1:
            return None

        header_rest = ser.read(HEADER_LEN - 2)  # version + seq + payload_len(2)
        if len(header_rest) < 4:
            return None

        payload_len = struct.unpack_from("<H", header_rest, 2)[0]
        if payload_len > 64:  # 防止把摄像头二进制流误判为传感器帧
            return None

        body = ser.read(payload_len + CRC_LEN)
        if len(body) < payload_len + CRC_LEN:
            return None

        frame = bytes([SYNC0, SYNC1]) + header_rest + body
        data_part = frame[:-2]
        recv_crc = struct.unpack_from("<H", frame, len(frame) - 2)[0]
        calc_crc = crc16_modbus(data_part)

        if recv_crc != calc_crc:
            log.warning("CRC 校验失败：期望 0x%04X，收到 0x%04X", calc_crc, recv_crc)
            return None

        return SerialPacket("sensor", frame)

    # 摄像头帧：标准 JPEG SOI/EOI。图片本身可能包含 55 AA，所以进入 JPEG 后不再解析传感器帧。
    if b0 == JPEG_SOI[:1]:
        b1 = ser.read(1)
        if not b1:
            return None
        if b1 != JPEG_SOI[1:]:
            return None

        jpg = bytearray(JPEG_SOI)
        prev = b1[0]
        while len(jpg) < max_jpeg_bytes:
            chunk = ser.read(1)
            if not chunk:
                return None
            cur = chunk[0]
            jpg.append(cur)
            if prev == JPEG_EOI[0] and cur == JPEG_EOI[1]:
                return SerialPacket("jpeg", bytes(jpg))
            prev = cur

        log.warning("JPEG 帧超过上限 %d 字节，已丢弃", max_jpeg_bytes)
        return None

    # 一些摄像头模块会把 JPEG 通过 base64/hex 文本行发出，而不是直接发二进制 JPEG。
    if 32 <= b0[0] <= 126:
        line = bytearray(b0)
        limit = max_jpeg_bytes * ASCII_LINE_LIMIT_FACTOR
        while len(line) < limit:
            chunk = ser.read(1)
            if not chunk:
                return None
            line.extend(chunk)
            if chunk in (b"\n", b"\r"):
                frame = decode_text_jpeg(bytes(line))
                if frame is not None and len(frame) <= max_jpeg_bytes:
                    return SerialPacket("jpeg", frame)
                return None

        log.warning("文本图片帧超过上限 %d 字节，已丢弃", limit)
        return None

    return None


def post_ingest(backend: str, data: dict, timeout: float = 5.0) -> tuple[bool, str]:
    url = backend.rstrip("/") + "/api/sensors/ingest"
    try:
        r = requests.post(url, json=data, timeout=timeout)
        if r.ok:
            return True, f"HTTP {r.status_code}"

        detail = r.text.strip()
        if len(detail) > 200:
            detail = detail[:200] + "..."
        return False, f"HTTP {r.status_code} {detail or '(empty body)'}"
    except requests.RequestException as e:
        return False, str(e)


def post_tank_frame(backend: str, frame: bytes, timeout: float = 5.0) -> tuple[bool, str]:
    url = backend.rstrip("/") + "/api/video/tank/ingest"
    try:
        r = requests.post(url, data=frame, headers={"Content-Type": "image/jpeg"}, timeout=timeout)
        if r.ok:
            return True, f"HTTP {r.status_code}"

        detail = r.text.strip()
        if len(detail) > 200:
            detail = detail[:200] + "..."
        return False, f"HTTP {r.status_code} {detail or '(empty body)'}"
    except requests.RequestException as e:
        return False, str(e)


def post_tank_detections(
    backend: str, detections: list[dict], timeout: float = 2.0
) -> tuple[bool, str]:
    url = backend.rstrip("/") + "/api/video/tank/detections"
    try:
        r = requests.post(url, json={"detections": detections}, timeout=timeout)
        if r.ok:
            return True, f"HTTP {r.status_code}"
        detail = r.text.strip()
        if len(detail) > 200:
            detail = detail[:200] + "..."
        return False, f"HTTP {r.status_code} {detail or '(empty body)'}"
    except requests.RequestException as e:
        return False, str(e)


def run_yolo_on_bgr_frame(model, frame, conf: float, device: str | None) -> list[dict]:
    """与 VideoController 约定一致：label + 归一化左上 xy + wh + score。"""
    h, w = frame.shape[:2]
    if w <= 0 or h <= 0:
        return []
    kw: dict = {"conf": conf, "verbose": False, "imgsz": 640}
    if device:
        kw["device"] = device
    results = model.predict(frame, **kw)
    dets: list[dict] = []
    for r in results:
        if r.boxes is None or len(r.boxes) == 0:
            continue
        nm = getattr(r, "names", None) or {}
        for box in r.boxes:
            x1, y1, x2, y2 = box.xyxy[0].tolist()
            cls_id = int(box.cls[0])
            score = float(box.conf[0])
            label = nm.get(cls_id, str(cls_id)) if isinstance(nm, dict) else str(cls_id)
            dets.append(
                {
                    "label": label,
                    "x": max(0.0, min(1.0, x1 / w)),
                    "y": max(0.0, min(1.0, y1 / h)),
                    "width": max(0.0, min(1.0, (x2 - x1) / w)),
                    "height": max(0.0, min(1.0, (y2 - y1) / h)),
                    "score": score,
                }
            )
    return dets


def maybe_infer_tank_and_post(
    backend: str,
    yolo_model,
    frame_bgr,
    conf: float,
    device: str | None,
    verbose: bool,
) -> None:
    if yolo_model is None or frame_bgr is None:
        return
    try:
        with _yolo_predict_lock:
            dets = run_yolo_on_bgr_frame(yolo_model, frame_bgr, conf, device)
        ok, reason = post_tank_detections(backend, dets, timeout=2.0)
        if not ok and verbose:
            log.warning("检测框上报失败：%s", reason)
    except Exception as e:
        if verbose:
            log.warning("YOLO 推理或上报异常：%s", e)


def run_tank_usb_camera(
    backend: str,
    camera_index: int,
    fps: float,
    verbose: bool,
    stop_event: threading.Event,
    yolo_model=None,
    yolo_conf: float = 0.25,
    yolo_device: str | None = None,
) -> None:
    try:
        import cv2
    except ImportError:
        log.error("未安装 OpenCV，无法读取 USB 摄像头。请执行：pip install opencv-python")
        return

    interval = 1.0 / max(fps, 0.1)
    log.info("鱼缸 USB 摄像头：index=%d，目标 %.1f FPS", camera_index, fps)

    while not stop_event.is_set():
        cap = cv2.VideoCapture(camera_index, cv2.CAP_DSHOW)
        if not cap.isOpened():
            cap.release()
            cap = cv2.VideoCapture(camera_index)

        if not cap.isOpened():
            log.warning("无法打开鱼缸 USB 摄像头 index=%d，3 秒后重试", camera_index)
            stop_event.wait(3.0)
            continue

        log.info("鱼缸 USB 摄像头已打开：index=%d", camera_index)
        last_log_at = 0.0
        try:
            while not stop_event.is_set():
                ok, frame = cap.read()
                if not ok or frame is None:
                    log.warning("读取鱼缸 USB 摄像头失败，准备重连")
                    break

                ok, jpg = cv2.imencode(
                    ".jpg",
                    frame,
                    [int(cv2.IMWRITE_JPEG_QUALITY), JPEG_ENCODE_QUALITY],
                )
                if not ok:
                    log.warning("鱼缸 USB 摄像头 JPEG 编码失败")
                    stop_event.wait(interval)
                    continue

                payload = jpg.tobytes()
                posted, reason = post_tank_frame(backend, payload, timeout=2.0)
                now = time.monotonic()
                if not posted:
                    log.warning("鱼缸 USB 摄像头帧推送失败：%s", reason)
                elif verbose and now - last_log_at >= 2.0:
                    log.info("鱼缸 USB 摄像头帧推送成功：%d bytes", len(payload))
                    last_log_at = now

                if posted:
                    maybe_infer_tank_and_post(
                        backend, yolo_model, frame, yolo_conf, yolo_device, verbose
                    )

                stop_event.wait(interval)
        finally:
            cap.release()

        stop_event.wait(1.0)


def run(
    port: str,
    baud: int,
    backend: str,
    verbose: bool,
    max_jpeg_bytes: int,
    yolo_model=None,
    yolo_conf: float = 0.25,
    yolo_device: str | None = None,
) -> None:
    log.info("串口：%s @ %d 波特，后端：%s", port, baud, backend)
    with serial.Serial(port, baud, timeout=1.0) as ser:
        log.info("串口已打开，开始监听…")
        fail_streak = 0
        last_push_at = 0.0
        while True:
            packet = read_packet(ser, max_jpeg_bytes)
            if packet is None:
                fail_streak += 1
                if fail_streak % 20 == 0:
                    log.warning("连续 %d 次未能解析帧，检查串口连接、波特率和摄像头数据格式", fail_streak)
                continue

            fail_streak = 0
            if packet.kind == "jpeg":
                ok, reason = post_tank_frame(backend, packet.data)
                if not ok:
                    log.warning("鱼缸摄像头帧推送失败：%s，大小=%d bytes", reason, len(packet.data))
                elif verbose:
                    log.info("鱼缸摄像头帧推送成功：%d bytes", len(packet.data))
                if ok and yolo_model is not None:
                    try:
                        import numpy as np
                        import cv2

                        arr = np.frombuffer(packet.data, dtype=np.uint8)
                        img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
                        maybe_infer_tank_and_post(
                            backend, yolo_model, img, yolo_conf, yolo_device, verbose
                        )
                    except ImportError:
                        log.warning("YOLO 串口 JPEG 路径需要 numpy；pip install numpy opencv-python")
                continue

            frame = packet.data
            payload = frame[HEADER_LEN : HEADER_LEN + (len(frame) - HEADER_LEN - CRC_LEN)]

            try:
                parsed = parse_payload(payload)
            except ValueError as e:
                log.warning("解析 payload 失败：%s", e)
                continue

            if verbose:
                raw = parsed["raw"]
                log.info(
                    "水温=%.1f°C  空气=%.1f°C/%.1f%%RH  WQI=%d  土壤=%d%%  泵=%d%%  告警=0x%08X",
                    raw["water_temp"], raw["air_temp"], raw["air_humidity"],
                    raw["wqi"], raw["soil_pct"], raw["pump_pct"], raw["alarm_flags"],
                )

            ingest_data = parsed["ingest"]
            now = time.monotonic()
            if now - last_push_at < PUSH_INTERVAL_SEC:
                continue

            ok, reason = post_ingest(backend, ingest_data)
            last_push_at = now
            if not ok:
                log.warning("推送失败：%s，数据：%s", reason, ingest_data)
            else:
                log.info("推送成功（1s 节流）：%s", ingest_data)


def main() -> None:
    parser = argparse.ArgumentParser(description="MCU UART → Spring Boot 传感器桥接")
    parser.add_argument("--port", default="COM20", help="串口号（Windows: COM3，Linux: /dev/ttyUSB0）")
    parser.add_argument("--baud", type=int, default=115200, help="波特率，默认 115200")
    parser.add_argument("--backend", default="http://localhost:8090", help="Spring Boot 后端地址")
    parser.add_argument("--verbose", action="store_true", help="打印每帧解析结果")
    parser.add_argument("--max-jpeg-bytes", type=int, default=DEFAULT_MAX_JPEG_BYTES, help="单张 JPEG 最大字节数")
    parser.add_argument("--tank-camera-index", type=int, default=DEFAULT_TANK_CAMERA_INDEX, help="鱼缸 USB 摄像头索引；设为 -1 可禁用")
    parser.add_argument("--tank-camera-fps", type=float, default=DEFAULT_CAMERA_FPS, help="鱼缸 USB 摄像头推流 FPS")
    parser.add_argument(
        "--no-yolo",
        action="store_true",
        help="禁用鱼缸 YOLO 检测（不加载模型、不 POST /api/video/tank/detections）",
    )
    parser.add_argument(
        "--yolo-weights",
        default=str(_DEFAULT_YOLO_WEIGHTS),
        help="YOLO .pt 权重路径；默认使用仓库 model/yolo_fish/.../best.pt",
    )
    parser.add_argument("--yolo-conf", type=float, default=0.25, help="检测置信度阈值")
    parser.add_argument(
        "--yolo-device",
        default="",
        help="推理设备：cpu、0、cuda:0 等；留空则由 Ultralytics 自动选择",
    )
    args = parser.parse_args()

    yolo_model = None
    yolo_device = args.yolo_device.strip() or None
    if not args.no_yolo:
        wpath = Path(args.yolo_weights)
        if wpath.is_file():
            try:
                from ultralytics import YOLO

                yolo_model = YOLO(str(wpath.resolve()))
                log.info("鱼缸 YOLO 已加载：%s", wpath)
            except ImportError:
                log.error("未安装 ultralytics，无法做鱼缸检测。请执行：pip install ultralytics")
        else:
            log.warning("未找到 YOLO 权重（已跳过检测）：%s", wpath)

    stop_event = threading.Event()
    if args.tank_camera_index >= 0:
        camera_thread = threading.Thread(
            target=run_tank_usb_camera,
            args=(
                args.backend,
                args.tank_camera_index,
                args.tank_camera_fps,
                args.verbose,
                stop_event,
                yolo_model,
                args.yolo_conf,
                yolo_device,
            ),
            name="tank-usb-camera",
            daemon=True,
        )
        camera_thread.start()

    while True:
        try:
            run(
                args.port,
                args.baud,
                args.backend,
                args.verbose,
                args.max_jpeg_bytes,
                yolo_model,
                args.yolo_conf,
                yolo_device,
            )
        except serial.SerialException as e:
            log.error("串口错误：%s，5 秒后重试…", e)
            time.sleep(5)
        except KeyboardInterrupt:
            stop_event.set()
            log.info("退出")
            break


if __name__ == "__main__":
    from aqua_system import run_serial

    run_serial()
