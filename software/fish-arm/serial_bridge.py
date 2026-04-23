"""
serial_bridge.py — MCU UART → Spring Boot 桥接脚本
================================================
读取 hardware/demo_wyr 固件（Communicate_Task_entry.c）发出的二进制上行帧，
解析后 POST 到 /api/sensors/ingest，后端前端轮询 /api/sensors 即可得到真实数据。

依赖：
    pip install pyserial requests

用法：
    python serial_bridge.py --port COM3 --backend http://localhost:8090
    python serial_bridge.py --port /dev/ttyUSB0 --backend http://127.0.0.1:8090

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
import struct
import time
import logging

import requests
import serial

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("bridge")

SYNC0 = 0x55
SYNC1 = 0xAA
HEADER_LEN = 6   # sync0 + sync1 + version + seq + payload_len(2)
CRC_LEN = 2
EXPECTED_PAYLOAD_LEN = 30


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


def read_frame(ser: serial.Serial) -> bytes | None:
    """从串口读取并验证一帧，返回完整帧字节（含 CRC）或 None。"""
    # 对齐同步头
    b0 = ser.read(1)
    if not b0 or b0[0] != SYNC0:
        return None
    b1 = ser.read(1)
    if not b1 or b1[0] != SYNC1:
        return None

    header_rest = ser.read(HEADER_LEN - 2)  # version + seq + payload_len(2)
    if len(header_rest) < 4:
        return None

    payload_len = struct.unpack_from("<H", header_rest, 2)[0]
    if payload_len > 64:  # 防止超大帧
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

    return frame


def post_ingest(backend: str, data: dict, timeout: float = 2.0) -> bool:
    url = backend.rstrip("/") + "/api/sensors/ingest"
    try:
        r = requests.post(url, json=data, timeout=timeout)
        return r.status_code == 200
    except requests.RequestException as e:
        log.warning("POST 失败：%s", e)
        return False


def run(port: str, baud: int, backend: str, verbose: bool) -> None:
    log.info("串口：%s @ %d 波特，后端：%s", port, baud, backend)
    with serial.Serial(port, baud, timeout=1.0) as ser:
        log.info("串口已打开，开始监听…")
        fail_streak = 0
        while True:
            frame = read_frame(ser)
            if frame is None:
                fail_streak += 1
                if fail_streak % 20 == 0:
                    log.warning("连续 %d 次未能解析帧，检查串口连接和波特率", fail_streak)
                continue

            fail_streak = 0
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

            ok = post_ingest(backend, parsed["ingest"])
            if not ok:
                log.warning("推送失败，数据：%s", parsed["ingest"])


def main() -> None:
    parser = argparse.ArgumentParser(description="MCU UART → Spring Boot 传感器桥接")
    parser.add_argument("--port", default="COM3", help="串口号（Windows: COM3，Linux: /dev/ttyUSB0）")
    parser.add_argument("--baud", type=int, default=115200, help="波特率，默认 115200")
    parser.add_argument("--backend", default="http://localhost:8090", help="Spring Boot 后端地址")
    parser.add_argument("--verbose", action="store_true", help="打印每帧解析结果")
    args = parser.parse_args()

    while True:
        try:
            run(args.port, args.baud, args.backend, args.verbose)
        except serial.SerialException as e:
            log.error("串口错误：%s，5 秒后重试…", e)
            time.sleep(5)
        except KeyboardInterrupt:
            log.info("退出")
            break


if __name__ == "__main__":
    main()
