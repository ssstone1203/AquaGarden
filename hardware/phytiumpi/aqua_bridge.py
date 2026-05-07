#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Phytium Pi Aqua Bridge (minimal)
================================
为 Spring Boot `/api/aqua/*` 提供下游 Bridge 能力，重点支持水泵控制。

已实现:
  - GET  /api/status
  - POST /api/pump/start   {"pwm": 80}
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

from flask import Flask, jsonify, request

app = Flask(__name__)

CLI = os.getenv("AQUA_SPI_CLI", "/opt/aqua/bin/aqua_spi_cli")
HOST = os.getenv("AQUA_BRIDGE_HOST", "0.0.0.0")
PORT = int(os.getenv("AQUA_BRIDGE_PORT", "18080"))
CMD_TIMEOUT_SEC = float(os.getenv("AQUA_BRIDGE_CMD_TIMEOUT_SEC", "8"))
DEFAULT_PUMP_PWM = int(os.getenv("AQUA_BRIDGE_DEFAULT_PUMP_PWM", "80"))

_state_lock = threading.Lock()
_pump_pwm = DEFAULT_PUMP_PWM
_pump_manual_on = False
_last_error = ""
_busy = False
_started_at = time.time()


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


@app.get("/api/status")
def status():
    ping = _run_cli(["sys", "ping"], timeout=3.0)
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
                "camera": {"hasRgb": False, "hasDepth": False},
                "lastError": _last_error if _last_error else (None if ping.ok else (ping.err or ping.out)),
                "uptimeSec": uptime_sec,
                "pump": {"manualOn": _pump_manual_on, "pwm": _pump_pwm},
            }
        )


@app.post("/api/pump/start")
def pump_start():
    global _pump_pwm, _pump_manual_on
    body = request.get_json(silent=True) or {}
    pwm = _clamp_pwm(_parse_int(body.get("pwm", _pump_pwm), _pump_pwm))
    r = _run_cli(["pump", "start", str(pwm)])
    if r.ok:
        with _state_lock:
            _pump_pwm = pwm
            _pump_manual_on = True
    return _json_bridge_result(r, {"pwm": pwm})


@app.post("/api/pump/pwm")
def pump_pwm():
    global _pump_pwm
    body = request.get_json(silent=True) or {}
    pwm = _clamp_pwm(_parse_int(body.get("pwm", _pump_pwm), _pump_pwm))
    r = _run_cli(["pump", "pwm", str(pwm)])
    if r.ok:
        with _state_lock:
            _pump_pwm = pwm
    return _json_bridge_result(r, {"pwm": pwm})


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
    global _pump_pwm, _pump_manual_on
    body = request.get_json(silent=True) or {}
    on = _parse_int(body.get("on", 1), 1)
    if on not in (0, 1):
        return _json_error("on must be 0 or 1")
    pwm = _clamp_pwm(_parse_int(body.get("pwm", _pump_pwm), _pump_pwm))
    r = _run_cli(["pump", "manual", str(on), str(pwm)])
    if r.ok:
        with _state_lock:
            _pump_pwm = pwm
            _pump_manual_on = on == 1
    return _json_bridge_result(r, {"on": on, "pwm": pwm})


@app.post("/api/pump/stop")
def pump_stop():
    global _pump_pwm, _pump_manual_on
    r = _run_cli(["pump", "stop"])
    if r.ok:
        with _state_lock:
            _pump_pwm = 0
            _pump_manual_on = True
    return _json_bridge_result(r)


@app.post("/api/pump/pulse")
def pump_pulse():
    body = request.get_json(silent=True) or {}
    seconds = _parse_int(body.get("seconds", 5), 5)
    pwm = _clamp_pwm(_parse_int(body.get("pwm", _pump_pwm), _pump_pwm))
    seconds = max(1, min(120, seconds))

    def _do_pulse():
        global _busy
        with _state_lock:
            _busy = True
        try:
            start = _run_cli(["pump", "start", str(pwm)])
            if not start.ok:
                return
            time.sleep(seconds)
            _run_cli(["pump", "stop"])
        finally:
            with _state_lock:
                _busy = False

    t = threading.Thread(target=_do_pulse, name="pump-pulse", daemon=True)
    t.start()
    return jsonify({"ok": True, "message": "pulse started", "seconds": seconds, "pwm": pwm})


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
    app.run(host=HOST, port=PORT, debug=False)
