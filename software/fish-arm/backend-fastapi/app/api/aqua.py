from fastapi import APIRouter, Depends, HTTPException, Response
from fastapi.responses import StreamingResponse

from app.core.security import get_current_user
from app.schemas.common import AtomizerControlRequest, UsbLightControlRequest
from app.services import bridge
from app.services.hardware_serial import hardware_serial
from app.services.video import BOUNDARY, generated_stream


router = APIRouter()
TASKS = {"feed", "loosen", "prune", "stop"}
VIDEO_MODES = {"rgb", "depth"}


@router.get("/api/aqua/status", dependencies=[Depends(get_current_user)])
async def status() -> dict:
    bridge_state = await bridge.bridge_status()
    if not hardware_serial.enabled:
        return bridge_state

    serial_state = hardware_serial.status(bridge.camera_status())
    merged = {
        **bridge_state,
        "ok": bool(bridge_state.get("ok") or serial_state.get("ok")),
        "connected": bool(bridge_state.get("connected") or serial_state.get("connected")),
        "pump": serial_state.get("pump"),
        "atomizer": serial_state.get("atomizer"),
        "usbLight": serial_state.get("usbLight"),
        "bridge": bridge_state,
        "hardwareSerial": serial_state,
    }
    return merged


@router.get("/api/aqua/pump/status")
def pump_status() -> dict:
    return hardware_serial.pump_debug_status()


@router.post("/api/aqua/atomizer", dependencies=[Depends(get_current_user)])
def atomizer_control(body: AtomizerControlRequest) -> Response:
    status_code, payload = hardware_serial.atomizer_set(body.state)
    return Response(content=_json_bytes(payload), status_code=status_code, media_type="application/json")


@router.post("/api/aqua/usb-light", dependencies=[Depends(get_current_user)])
def usb_light_control(body: UsbLightControlRequest) -> Response:
    status_code, payload = hardware_serial.usb_light_set(body.mode)
    return Response(content=_json_bytes(payload), status_code=status_code, media_type="application/json")


@router.post("/api/aqua/tasks/{task}", dependencies=[Depends(get_current_user)])
async def task(task: str):
    if task not in TASKS:
        raise HTTPException(status_code=400, detail=f"unknown task: {task}")
    return await _bridge_response("POST", f"/api/task/{task}")


@router.post("/api/aqua/arm/home", dependencies=[Depends(get_current_user)])
async def arm_home():
    return await _bridge_response("POST", "/api/arm/home")


@router.post("/api/aqua/arm/pose", dependencies=[Depends(get_current_user)])
async def arm_pose(body: dict):
    return await _bridge_response("POST", "/api/arm/pose", body)


@router.post("/api/aqua/arm/gripper", dependencies=[Depends(get_current_user)])
async def arm_gripper(body: dict):
    return await _bridge_response("POST", "/api/arm/gripper", body)


@router.post("/api/aqua/rail/position", dependencies=[Depends(get_current_user)])
async def rail_position(body: dict):
    position = _bounded_int(body.get("position"), 0, 5200, "position must be 0..5200")
    return await _bridge_response("POST", "/api/rail/position", {"position": position})


@router.post("/api/aqua/rail/move", dependencies=[Depends(get_current_user)])
async def rail_move(body: dict):
    return await rail_position(body)


@router.post("/api/aqua/pump/start", dependencies=[Depends(get_current_user)])
async def pump_start(body: dict):
    pwm = _pwm(body.get("pwm"))
    return await _pump_response("start", {"pwm": pwm}, lambda: hardware_serial.pump_start(pwm))


@router.post("/api/aqua/pump/pwm", dependencies=[Depends(get_current_user)])
async def pump_pwm(body: dict):
    pwm = _pwm(body.get("pwm"))
    return await _pump_response("pwm", {"pwm": pwm}, lambda: hardware_serial.pump_pwm(pwm))


@router.post("/api/aqua/pump/auto", dependencies=[Depends(get_current_user)])
async def pump_auto():
    return await _pump_response("auto", None, hardware_serial.pump_auto)


@router.post("/api/aqua/pump/manual", dependencies=[Depends(get_current_user)])
async def pump_manual(body: dict):
    on = _bounded_int(body.get("on"), 0, 1, "on must be 0 or 1")
    pwm = _pwm(body.get("pwm"))
    return await _pump_response("manual", {"on": on, "pwm": pwm}, lambda: hardware_serial.pump_manual(on, pwm))


@router.post("/api/aqua/pump/stop", dependencies=[Depends(get_current_user)])
async def pump_stop():
    return await _pump_response("stop", None, hardware_serial.pump_stop)


@router.post("/api/aqua/pump/pulse", dependencies=[Depends(get_current_user)])
async def pump_pulse(body: dict | None = None):
    body = body or {}
    seconds = _bounded_int(body.get("seconds", 5), 1, 120, "seconds must be 1..120")
    pwm = _pwm(body.get("pwm", 80))
    return await _pump_response("pulse", {"seconds": seconds, "pwm": pwm}, lambda: hardware_serial.pump_pulse(seconds, pwm))


@router.post("/api/control/pump", dependencies=[Depends(get_current_user)])
async def control_pump(body: dict | None = None) -> dict:
    body = body or {}
    seconds = max(1, min(120, _int_or_default(body.get("seconds"), 5)))
    pwm = max(0, min(100, _int_or_default(body.get("pwm"), 80)))
    if hardware_serial.enabled:
        status_code, payload = hardware_serial.pump_pulse(seconds, pwm)
        if 200 <= status_code < 300:
            return {"ok": True, "seconds": seconds, "pwm": pwm, "status": status_code}
        return {"ok": False, "seconds": seconds, "message": payload.get("message", "水泵请求失败")}
    status_code, response = await bridge.bridge_request("POST", "/api/pump/pulse", {"seconds": seconds, "pwm": pwm})
    if 200 <= status_code < 300:
        return {"ok": True, "seconds": seconds, "pwm": pwm, "status": status_code}
    message = response.get("message") if isinstance(response, dict) else f"水泵请求失败 HTTP {status_code}"
    return {"ok": False, "seconds": seconds, "message": message}


@router.get("/api/aqua/video/{mode}")
async def video(mode: str):
    if mode not in VIDEO_MODES:
        raise HTTPException(status_code=400, detail=f"unknown video mode: {mode}")
    source = bridge.camera_source(mode)
    if source:
        return StreamingResponse(bridge.proxy_stream(source), media_type=f"multipart/x-mixed-replace; boundary={BOUNDARY}")
    return StreamingResponse(generated_stream(mode.upper(), "stream proxy", 1.0), media_type=f"multipart/x-mixed-replace; boundary={BOUNDARY}")


async def _bridge_response(method: str, path: str, body: dict | None = None):
    status_code, payload = await bridge.bridge_request(method, path, body)
    return Response(content=_json_bytes(payload), status_code=status_code, media_type="application/json")


async def _pump_response(action: str, body: dict | None, serial_call):
    if hardware_serial.enabled:
        status_code, payload = serial_call()
        return Response(content=_json_bytes(payload), status_code=status_code, media_type="application/json")
    path = f"/api/pump/{action}"
    if bridge.settings.serial_pump_enabled:
        status_code, payload = await bridge.serial_pump_request("POST", path, body)
        return Response(content=_json_bytes(payload), status_code=status_code, media_type="application/json")
    status_code, payload = await bridge.bridge_request("POST", path, body)
    return Response(content=_json_bytes(payload), status_code=status_code, media_type="application/json")


def _json_bytes(payload) -> bytes:
    import json

    return json.dumps(payload, ensure_ascii=False).encode("utf-8")


def _pwm(value) -> int:
    return _bounded_int(value, 0, 100, "pwm must be 0..100")


def _bounded_int(value, min_value: int, max_value: int, message: str) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError) as exc:
        raise HTTPException(status_code=400, detail=message) from exc
    if parsed < min_value or parsed > max_value:
        raise HTTPException(status_code=400, detail=message)
    return parsed


def _int_or_default(value, default: int) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default
