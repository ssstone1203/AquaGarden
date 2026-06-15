from fastapi import APIRouter, Depends, Response

from app.core.security import get_current_user
from app.services.state import state


router = APIRouter()
CMD_STOP = 0x01
CMD_START = 0x02
CMD_SET_PWM = 0x03


@router.post("/api/mcu/pump", dependencies=[Depends(get_current_user)])
def enqueue(body: dict) -> dict:
    action = str(body.get("action", "")).lower()
    power = _parse_int(body.get("power"), 0)
    cmd = {"stop": CMD_STOP, "start": CMD_START, "set_pwm": CMD_SET_PWM}.get(action, -1)
    if cmd < 0:
        return {"ok": False, "message": f"未知 action: {action}，支持 stop / start / set_pwm"}
    state.enqueue_pump(cmd, power)
    return {"ok": True, "action": action, "power": max(0, min(100, power))}


@router.get("/api/mcu/pump/pending")
def poll_pending(response: Response):
    cmd = state.drain_pump()
    if cmd is None:
        response.status_code = 204
        return None
    return {"cmd": cmd.cmd, "power": cmd.power, "cmdName": cmd.cmd_name}


@router.get("/api/mcu/pump/status")
def status() -> dict:
    cmd = state.peek_pump()
    if cmd is None:
        return {"pending": False}
    return {"pending": True, "cmd": cmd.cmd, "power": cmd.power, "cmdName": cmd.cmd_name}


def _parse_int(value, default: int) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default
