#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Phytium Pi Aqua Bridge (minimal)
================================
为 Spring Boot `/api/aqua/*` 提供下游 Bridge 能力，重点支持水泵控制。

已实现:
  - GET  /api/status
  - POST /api/pump/start   {"pwm": 80}   # pwm 为「界面语义」0=关 100=满；Bridge 换算为 SPI 占空比
  - POST /api/pump/pwm     {"pwm": 60}
  - POST /api/pump/auto
  - POST /api/pump/manual  {"on": 1, "pwm": 70}
  - POST /api/pump/stop    # aqua_spi_cli pump stop（关泵并保持手动 0%，勿用 manual 0 0）
  - POST /api/pump/pulse   {"seconds": 5}

其中 pulse 会执行：
  1) aqua_spi_cli pump start <pwm>
  2) sleep <seconds>
  3) aqua_spi_cli pump stop
"""

from __future__ import annotations

import os
import subprocess
import threading
import time
from dataclasses import dataclass
from typing import Any

from flask import Flask, Response, jsonify, request, stream_with_context

app = Flask(__name__)

CLI = os.getenv("AQUA_SPI_CLI", "/opt/aqua/bin/aqua_spi_cli")
HOST = os.getenv("AQUA_BRIDGE_HOST", "0.0.0.0")
PORT = int(os.getenv("AQUA_BRIDGE_PORT", "18080"))
CMD_TIMEOUT_SEC = float(os.getenv("AQUA_BRIDGE_CMD_TIMEOUT_SEC", "8"))
DEFAULT_PUMP_PWM_UI = max(0, min(100, int(os.getenv("AQUA_BRIDGE_DEFAULT_PUMP_PWM", "80"))))
RGB_CAMERA_INDEX = int(os.getenv("AQUA_BRIDGE_RGB_CAMERA_INDEX", "0"))
DEPTH_CAMERA_INDEX = int(os.getenv("AQUA_BRIDGE_DEPTH_CAMERA_INDEX", "-1"))
CAMERA_WIDTH = int(os.getenv("AQUA_BRIDGE_CAMERA_WIDTH", "640"))
CAMERA_HEIGHT = int(os.getenv("AQUA_BRIDGE_CAMERA_HEIGHT", "480"))
CAMERA_FPS = float(os.getenv("AQUA_BRIDGE_CAMERA_FPS", "15"))
CAMERA_JPEG_QUALITY = max(30, min(95, int(os.getenv("AQUA_BRIDGE_CAMERA_JPEG_QUALITY", "80"))))
MJPEG_BOUNDARY = "frame"

_state_lock = threading.Lock()
# 与前端滑块一致：0=关、100=最大；SPI 侧占空比由 _ui_pwm_to_spi_percent 换算
_pump_pwm_ui = DEFAULT_PUMP_PWM_UI
_pump_manual_on = False
_last_error = ""
_busy = False
_started_at = time.time()
_camera_state: dict[str, dict[str, Any]] = {
    "rgb": {"ok": False, "lastFrameAt": 0.0, "lastError": ""},
    "depth": {"ok": False, "lastFrameAt": 0.0, "lastError": "depth camera disabled"},
}


@dataclass
class CmdResult:
    ok: bool
    code: int
    out: str
    err: str
    cmd: list[str]


def _set_error(msg: str) -> None:
    global _last_error
    with _state_lock:
        _last_error = msg


def _clear_error() -> None:
    global _last_error
    with _state_lock:
        _last_error = ""


def _set_camera_state(mode: str, ok: bool, error: str = "") -> None:
    with _state_lock:
        st = _camera_state.setdefault(mode, {"ok": False, "lastFrameAt": 0.0, "lastError": ""})
        st["ok"] = ok
        if ok:
            st["lastFrameAt"] = time.time()
            st["lastError"] = ""
        else:
            st["lastError"] = error


def _camera_status(mode: str) -> dict[str, Any]:
    with _state_lock:
        st = dict(_camera_state.get(mode, {}))
    last_frame_at = float(st.get("lastFrameAt") or 0.0)
    return {
        "ok": bool(st.get("ok")),
        "lastFrameAt": int(last_frame_at * 1000) if last_frame_at else 0,
        "ageSec": round(time.time() - last_frame_at, 1) if last_frame_at else None,
        "lastError": st.get("lastError") or "",
    }


def _run_cli(args: list[str], timeout: float | None = None) -> CmdResult:
    cmd = [CLI, *args]
    t = timeout if timeout is not None else CMD_TIMEOUT_SEC
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=t,
            check=False,
        )
        ok = proc.returncode == 0
        if not ok:
            _set_error(f"cmd failed({proc.returncode}): {' '.join(cmd)} :: {proc.stderr.strip()}")
        else:
            _clear_error()
        return CmdResult(ok=ok, code=proc.returncode, out=proc.stdout.strip(), err=proc.stderr.strip(), cmd=cmd)
    except Exception as e:
        _set_error(f"cmd exception: {' '.join(cmd)} :: {e}")
        return CmdResult(ok=False, code=500, out="", err=str(e), cmd=cmd)


def _parse_int(value: Any, default: int) -> int:
    try:
        return int(value)
    except Exception:
        return default


def _clamp_pwm(v: int) -> int:
    return max(0, min(100, v))


def _ui_pwm_to_spi_percent(ui: int) -> int:
    """
    界面 0=关、100=满；硬件/SPI 侧占空比与界面数值相反：spi = 100 - ui。
    固件对 pump start 且 payload=0 会误用默认 60%，故 ui=100 时必须下发 spi=100。
    ui=0 仅用于「关泵」，调用方应走 pump stop，不应把 0 传给本函数参与 start。
    """
    ui = _clamp_pwm(ui)
    if ui <= 0:
        return 0
    spi = 100 - ui
    if spi <= 0:
        return 100
    return spi


def _json_error(message: str, code: int = 400):
    return jsonify({"ok": False, "message": message}), code


def _json_bridge_result(r: CmdResult, extra: dict[str, Any] | None = None, fail_http: int = 502):
    payload: dict[str, Any] = {
        "ok": r.ok,
        "code": r.code,
        "message": "ok" if r.ok else (r.err or r.out or "command failed"),
        "stdout": r.out,
        "stderr": r.err,
        "cmd": " ".join(r.cmd),
    }
    if extra:
        payload.update(extra)
    if r.ok:
        return jsonify(payload)
    return jsonify(payload), fail_http


def _load_cv2():
    try:
        import cv2  # type: ignore
        import numpy as np  # type: ignore

        return cv2, np, None
    except Exception as e:
        return None, None, str(e)


def _placeholder_jpeg(mode: str, message: str) -> bytes | None:
    cv2, np, err = _load_cv2()
    if cv2 is None or np is None:
        _set_camera_state(mode, False, f"opencv/numpy unavailable: {err}")
        return None

    img = np.zeros((360, 640, 3), dtype=np.uint8)
    img[:, :] = (35, 24, 15)
    cv2.putText(img, "Aqua Bridge Camera", (28, 62), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (230, 245, 255), 2)
    cv2.putText(img, mode.upper(), (28, 108), cv2.FONT_HERSHEY_SIMPLEX, 0.75, (80, 220, 255), 2)
    cv2.putText(img, message[:58], (28, 156), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (120, 180, 255), 1)
    cv2.putText(img, time.strftime("%H:%M:%S"), (28, 206), cv2.FONT_HERSHEY_SIMPLEX, 0.65, (180, 190, 200), 1)
    ok, buf = cv2.imencode(".jpg", img, [int(cv2.IMWRITE_JPEG_QUALITY), CAMERA_JPEG_QUALITY])
    return buf.tobytes() if ok else None


def _mjpeg_part(jpeg: bytes) -> bytes:
    return (
        b"--" + MJPEG_BOUNDARY.encode("ascii")
        + b"\r\nContent-Type: image/jpeg\r\nContent-Length: "
        + str(len(jpeg)).encode("ascii")
        + b"\r\n\r\n"
        + jpeg
        + b"\r\n"
    )


def _camera_index_for_mode(mode: str) -> int:
    return DEPTH_CAMERA_INDEX if mode == "depth" else RGB_CAMERA_INDEX


def _camera_stream(mode: str):
    cv2, _np, err = _load_cv2()
    if cv2 is None:
        _set_camera_state(mode, False, f"opencv unavailable: {err}")
        jpeg = _placeholder_jpeg(mode, "Install opencv-python to enable camera streaming")
        if jpeg is None:
            yield b""
            return
        while True:
            yield _mjpeg_part(jpeg)
            time.sleep(1.0)

    camera_index = _camera_index_for_mode(mode)
    if camera_index < 0:
        msg = f"{mode} camera disabled: set AQUA_BRIDGE_{mode.upper()}_CAMERA_INDEX"
        _set_camera_state(mode, False, msg)
        jpeg = _placeholder_jpeg(mode, msg)
        while jpeg is not None:
            yield _mjpeg_part(jpeg)
            time.sleep(1.0)
            jpeg = _placeholder_jpeg(mode, msg)
        return

    interval = 1.0 / max(CAMERA_FPS, 0.1)
    while True:
        cap = cv2.VideoCapture(camera_index)
        if not cap.isOpened():
            cap.release()
            msg = f"cannot open camera index={camera_index}"
            _set_camera_state(mode, False, msg)
            jpeg = _placeholder_jpeg(mode, msg)
            if jpeg is not None:
                yield _mjpeg_part(jpeg)
            time.sleep(2.0)
            continue

        cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAMERA_WIDTH)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
        cap.set(cv2.CAP_PROP_FPS, CAMERA_FPS)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        try:
            cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        except Exception:
            pass
        try:
            while True:
                frame_started = time.monotonic()
                ok, frame = cap.read()
                if not ok or frame is None:
                    _set_camera_state(mode, False, f"camera read failed index={camera_index}")
                    break
                ok, buf = cv2.imencode(
                    ".jpg",
                    frame,
                    [int(cv2.IMWRITE_JPEG_QUALITY), CAMERA_JPEG_QUALITY],
                )
                if not ok:
                    _set_camera_state(mode, False, "jpeg encode failed")
                    time.sleep(interval)
                    continue
                _set_camera_state(mode, True)
                yield _mjpeg_part(buf.tobytes())
                elapsed = time.monotonic() - frame_started
                time.sleep(max(0.001, interval - elapsed))
        finally:
            cap.release()


@app.get("/api/status")
def status():
    ping = _run_cli(["sys", "ping"], timeout=3.0)
    rgb_status = _camera_status("rgb")
    depth_status = _camera_status("depth")
    with _state_lock:
        uptime_sec = int(time.time() - _started_at)
        return jsonify(
            {
                "ok": ping.ok,
                "connected": ping.ok,
                "busy": _busy,
                "currentTask": "idle",
                "phase": "idle",
                "railPosition": None,
                "servoPulse": None,
                "camera": {
                    "hasRgb": rgb_status["ok"],
                    "hasDepth": depth_status["ok"],
                    "rgb": rgb_status,
                    "depth": depth_status,
                },
                "lastError": _last_error if _last_error else (None if ping.ok else (ping.err or ping.out)),
                "uptimeSec": uptime_sec,
                "pump": {
                    "manualOn": _pump_manual_on,
                    # 自动模式不下发 pwm，避免前端轮询把滑块同步成「硬件原始值」而误归零
                    "pwm": (_pump_pwm_ui if _pump_manual_on else None),
                },
            }
        )


@app.get("/video/<mode>.mjpg")
def video_stream(mode: str):
    if mode not in ("rgb", "depth"):
        return _json_error(f"unknown video mode: {mode}", 404)
    return Response(
        stream_with_context(_camera_stream(mode)),
        mimetype=f"multipart/x-mixed-replace; boundary={MJPEG_BOUNDARY}",
        headers={"Cache-Control": "no-store"},
    )


@app.post("/api/pump/start")
def pump_start():
    global _pump_pwm_ui, _pump_manual_on
    body = request.get_json(silent=True) or {}
    ui = _clamp_pwm(_parse_int(body.get("pwm", _pump_pwm_ui), _pump_pwm_ui))
    if ui <= 0:
        r = _run_cli(["pump", "stop"])
        if r.ok:
            with _state_lock:
                _pump_pwm_ui = 0
                _pump_manual_on = True
        return _json_bridge_result(r, {"pwmUi": 0, "pwm": 0})
    spi = _ui_pwm_to_spi_percent(ui)
    r = _run_cli(["pump", "start", str(spi)])
    if r.ok:
        with _state_lock:
            _pump_pwm_ui = ui
            _pump_manual_on = True
    return _json_bridge_result(r, {"pwmUi": ui, "pwmSpi": spi})


@app.post("/api/pump/pwm")
def pump_pwm():
    global _pump_pwm_ui, _pump_manual_on
    body = request.get_json(silent=True) or {}
    ui = _clamp_pwm(_parse_int(body.get("pwm", _pump_pwm_ui), _pump_pwm_ui))
    if ui <= 0:
        r = _run_cli(["pump", "stop"])
        if r.ok:
            with _state_lock:
                _pump_pwm_ui = 0
                _pump_manual_on = True
        return _json_bridge_result(r, {"pwmUi": 0, "pwm": 0})
    spi = _ui_pwm_to_spi_percent(ui)
    r = _run_cli(["pump", "pwm", str(spi)])
    if r.ok:
        with _state_lock:
            _pump_pwm_ui = ui
            _pump_manual_on = True
    return _json_bridge_result(r, {"pwmUi": ui, "pwmSpi": spi})


@app.post("/api/pump/auto")
def pump_auto():
    global _pump_manual_on
    r = _run_cli(["pump", "auto"])
    if r.ok:
        with _state_lock:
            _pump_manual_on = False
    return _json_bridge_result(r)


@app.post("/api/pump/manual")
def pump_manual():
    global _pump_pwm_ui, _pump_manual_on
    body = request.get_json(silent=True) or {}
    on = _parse_int(body.get("on", 1), 1)
    if on not in (0, 1):
        return _json_error("on must be 0 or 1")
    pwm_ui = _clamp_pwm(_parse_int(body.get("pwm", _pump_pwm_ui), _pump_pwm_ui))
    # on=0：固件 manual 0 0 易与「关泵」混淆，统一走 auto；on=1 且 pwm=0：关泵用 stop
    if on == 0:
        r = _run_cli(["pump", "auto"])
        if r.ok:
            with _state_lock:
                _pump_manual_on = False
        return _json_bridge_result(r, {"on": 0})
    if pwm_ui <= 0:
        r = _run_cli(["pump", "stop"])
        if r.ok:
            with _state_lock:
                _pump_pwm_ui = 0
                _pump_manual_on = True
        return _json_bridge_result(r, {"on": 1, "pwmUi": 0})
    spi = _ui_pwm_to_spi_percent(pwm_ui)
    r = _run_cli(["pump", "manual", "1", str(spi)])
    if r.ok:
        with _state_lock:
            _pump_pwm_ui = pwm_ui
            _pump_manual_on = True
    return _json_bridge_result(r, {"on": 1, "pwmUi": pwm_ui, "pwmSpi": spi})


@app.post("/api/pump/stop")
def pump_stop():
    global _pump_pwm_ui, _pump_manual_on
    r = _run_cli(["pump", "stop"])
    if r.ok:
        with _state_lock:
            _pump_pwm_ui = 0
            _pump_manual_on = True
    return _json_bridge_result(r)


@app.post("/api/pump/pulse")
def pump_pulse():
    body = request.get_json(silent=True) or {}
    seconds = _parse_int(body.get("seconds", 5), 5)
    pwm_ui = _clamp_pwm(_parse_int(body.get("pwm", _pump_pwm_ui), _pump_pwm_ui))
    seconds = max(1, min(120, seconds))
    spi = _ui_pwm_to_spi_percent(pwm_ui) if pwm_ui > 0 else 0

    def _do_pulse():
        global _busy
        with _state_lock:
            _busy = True
        try:
            if pwm_ui <= 0:
                return
            start = _run_cli(["pump", "start", str(spi)])
            if not start.ok:
                return
            time.sleep(seconds)
            _run_cli(["pump", "stop"])
        finally:
            with _state_lock:
                _busy = False

    t = threading.Thread(target=_do_pulse, name="pump-pulse", daemon=True)
    t.start()
    return jsonify({"ok": True, "message": "pulse started", "seconds": seconds, "pwmUi": pwm_ui, "pwmSpi": spi})


# 占位接口（当前未实现），避免上游 404 语义混淆
@app.post("/api/task/<task>")
def task_not_implemented(task: str):
    return _json_error(f"task not implemented on phytium bridge: {task}", 404)


@app.post("/api/arm/home")
def arm_home_not_implemented():
    return _json_error("arm home not implemented on phytium bridge", 404)


@app.post("/api/arm/pose")
def arm_pose_not_implemented():
    return _json_error("arm pose not implemented on phytium bridge", 404)


@app.post("/api/arm/gripper")
def arm_gripper_not_implemented():
    return _json_error("arm gripper not implemented on phytium bridge", 404)


@app.post("/api/rail/position")
def rail_not_implemented():
    return _json_error("rail control not implemented on phytium bridge", 404)


if __name__ == "__main__":
    from aqua_system import run_bridge

    run_bridge()
