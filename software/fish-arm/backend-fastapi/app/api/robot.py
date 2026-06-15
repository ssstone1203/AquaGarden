from fastapi import APIRouter, HTTPException

from app.schemas.common import RobotControlRequest
from app.services.logs import hub
from app.services.state import iso_now, state


router = APIRouter()
VALID = {"up", "down", "left", "right", "forward", "backward"}


@router.post("/api/robot/control")
async def control(body: RobotControlRequest) -> dict:
    if body.direction not in VALID:
        raise HTTPException(status_code=400, detail="Invalid direction")
    position = state.move_robot(body.direction)
    await hub.broadcast({"type": "robot", "message": f"机械臂移动: {body.direction}, 位置: {position}"})
    return {"status": "success", "position": position}


@router.get("/api/robot/status")
def status() -> dict:
    return {
        "connected": True,
        "currentTask": "待命",
        "uptime": "03:24:15",
        "position": dict(state.robot_position),
        "servoAngles": state.servo_angles(),
        "timestamp": iso_now(),
    }
