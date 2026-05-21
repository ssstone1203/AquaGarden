#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import logging
import os
import re
import subprocess
import sys
import time
import traceback
from typing import Any, Dict, Optional

import requests

# ========= 配置 =========
AQUA_CLI = os.getenv("AQUA_CLI", "aqua_spi_cli")
BACKEND_URL = os.getenv("BACKEND_URL", "http://10.116.177.24:8090/api/sensor/upload")
DEVICE_ID = os.getenv("DEVICE_ID", "phytium-01")
DEVICE_TOKEN = os.getenv("DEVICE_TOKEN", "123456789").strip()
POLL_INTERVAL_SEC = float(os.getenv("POLL_INTERVAL_SEC", "5"))
HTTP_TIMEOUT_SEC = float(os.getenv("HTTP_TIMEOUT_SEC", "5"))
SNAPSHOT_TIMEOUT_SEC = float(os.getenv("SNAPSHOT_TIMEOUT_SEC", "4"))

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")

SENSOR_UPLOAD_PLAN = [
    ("temp-01", "waterTempC", "C"),
    ("air-temp-01", "airTempC", "C"),
    ("humidity-01", "airHumidityPct", "%RH"),
    ("wqi-01", "wqi", "score"),
    ("soil-moisture-01", "soilMoisturePct", "%"),
]


def _decode_bytes(raw: bytes) -> str:
    # 兼容飞腾派上可能的 locale 非 UTF-8 场景
    for enc in ("utf-8", "gb18030", "latin-1"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("utf-8", errors="replace")


def _ascii_safe_text(text: str) -> str:
    return text.encode("unicode_escape").decode("ascii", errors="ignore")


def _latin1_header_value(name: str, value: str) -> str:
    """
    HTTP/1.1 头值最终会按 latin-1 编码，若包含中文/全角字符会抛 UnicodeEncodeError。
    这里提前校验并给出明确错误信息。
    """
    cleaned = value.strip().strip('"').strip("'")
    if not cleaned:
        raise RuntimeError(f"{name} is empty after trim")
    try:
        cleaned.encode("latin-1")
    except UnicodeEncodeError:
        escaped = _ascii_safe_text(cleaned)
        raise RuntimeError(f"{name} contains non-latin1 chars: {escaped}")
    return cleaned


def _extract_float(pattern: str, text: str) -> Optional[float]:
    m = re.search(pattern, text, re.MULTILINE)
    return float(m.group(1)) if m else None


def _extract_int(pattern: str, text: str) -> Optional[int]:
    m = re.search(pattern, text, re.MULTILINE)
    return int(m.group(1)) if m else None


def read_snapshot() -> Dict[str, Any]:
    """
    解析 aqua_spi_cli snapshot 输出。
    期待类似：
      空气   : 26.4 °C  58.2 %RH
      水温   : 22.1 °C
      土壤   : 38 % (need_watering=0)
      WQI    : 92
      水泵   : 0 %
      压力   : 1.20 / 0.85 / 0.31 kg
      告警位 : 0x00000000
    """
    proc = subprocess.run(
        [AQUA_CLI, "snapshot"],
        capture_output=True,
        timeout=SNAPSHOT_TIMEOUT_SEC,
        check=True,
    )
    out = _decode_bytes(proc.stdout)

    air_temp = _extract_float(r"空气\s*:\s*([-+]?\d+(?:\.\d+)?)\s*°C", out)
    air_humi = _extract_float(r"空气\s*:\s*[-+]?\d+(?:\.\d+)?\s*°C\s*([-+]?\d+(?:\.\d+)?)\s*%RH", out)
    water_temp = _extract_float(r"水温\s*:\s*([-+]?\d+(?:\.\d+)?)\s*°C", out)
    soil_pct = _extract_int(r"土壤\s*:\s*(\d+)\s*%", out)
    need_watering = _extract_int(r"need_watering\s*=\s*(\d+)", out)
    wqi = _extract_int(r"WQI\s*:\s*(\d+)", out)
    pump_power = _extract_int(r"水泵\s*:\s*(\d+)\s*%", out)

    m_press = re.search(
        r"压力\s*:\s*([-+]?\d+(?:\.\d+)?)\s*/\s*([-+]?\d+(?:\.\d+)?)\s*/\s*([-+]?\d+(?:\.\d+)?)\s*kg",
        out,
        re.MULTILINE,
    )
    p0 = float(m_press.group(1)) if m_press else None
    p1 = float(m_press.group(2)) if m_press else None
    p2 = float(m_press.group(3)) if m_press else None

    m_alarm = re.search(r"告警位\s*:\s*(0x[0-9A-Fa-f]+)", out, re.MULTILINE)
    alarm_flags = int(m_alarm.group(1), 16) if m_alarm else None

    data = {
        "airTempC": air_temp,
        "airHumidityPct": air_humi,
        "waterTempC": water_temp,
        "soilMoisturePct": soil_pct,
        "needWatering": need_watering,
        "wqi": wqi,
        "pumpPowerPct": pump_power,
        "pressureKg0": p0,
        "pressureKg1": p1,
        "pressureKg2": p2,
        "alarmFlags": alarm_flags,
    }

    core_keys = ["airTempC", "airHumidityPct", "soilMoisturePct", "wqi", "waterTempC"]
    if not any(data.get(k) is not None for k in core_keys):
        safe_out = _ascii_safe_text(out)
        raise RuntimeError(f"snapshot parse failed, output(escaped)={safe_out}")

    return data


def upload_one(sensor_id: str, value: Any, unit: str, ts_ms: int) -> None:
    if value is None:
        raise RuntimeError(f"{sensor_id} value is None")
    if not isinstance(value, (int, float)):
        raise RuntimeError(f"{sensor_id} value is not numeric: {value}")

    payload = {
        "deviceId": DEVICE_ID,
        "sensorId": sensor_id,
        "value": float(value),
        "unit": unit,
        "ts": ts_ms,
    }
    token = _latin1_header_value("DEVICE_TOKEN", DEVICE_TOKEN)
    headers = {
        "Content-Type": "application/json",
        "X-Device-Token": token,
    }

    resp = requests.post(BACKEND_URL, json=payload, headers=headers, timeout=HTTP_TIMEOUT_SEC)
    if resp.status_code // 100 != 2:
        safe_resp_text = _ascii_safe_text(resp.text or "")
        raise RuntimeError(f"{sensor_id} upload failed: {resp.status_code} body(escaped)={safe_resp_text}")


def upload_snapshot(data: Dict[str, Any]) -> None:
    ts_ms = int(time.time() * 1000)
    for sensor_id, field_name, unit in SENSOR_UPLOAD_PLAN:
        upload_one(sensor_id, data.get(field_name), unit, ts_ms)


def main() -> None:
    # 终端 locale 不是 UTF-8 时，避免 logging 输出中文触发编码异常
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
        except Exception:
            pass

    if not DEVICE_TOKEN:
        raise RuntimeError("DEVICE_TOKEN is empty, please export DEVICE_TOKEN first.")

    logging.info("start uploader: cli=%s, url=%s, deviceId=%s", AQUA_CLI, BACKEND_URL, DEVICE_ID)
    while True:
        try:
            logging.debug("step=read_snapshot begin")
            data = read_snapshot()
            logging.debug("step=read_snapshot ok")
            logging.debug("step=upload_snapshot begin")
            upload_snapshot(data)
            logging.debug("step=upload_snapshot ok")
            logging.info(
                "ok water=%.1fC air=%.1fC hum=%.1f%% soil=%s%% wqi=%s",
                data.get("waterTempC") or -1,
                data.get("airTempC") or -1,
                data.get("airHumidityPct") or -1,
                data.get("soilMoisturePct"),
                data.get("wqi"),
            )
        except Exception as e:
            logging.error("cycle failed: %s", _ascii_safe_text(str(e)))
            tb = _ascii_safe_text(traceback.format_exc())
            logging.error("traceback(escaped): %s", tb)

        time.sleep(POLL_INTERVAL_SEC)


if __name__ == "__main__":
    from aqua_system import run_sensor

    run_sensor()
