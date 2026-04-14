"""
Robot control and mode API router.
"""
from fastapi import APIRouter, Depends, HTTPException
import logging
from datetime import datetime
import json

from app.core.security import verify_token
from app.services.state import system_state
from app.schemas.robot import RobotControl, ModeSwitch, SystemStateResponse
from app.config import settings

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/robot", tags=["Robot"])


@router.post("/control")
async def control_robot(
    control: RobotControl, 
    username: str = Depends(verify_token)
):
    """Control robot arm movement."""
    direction_map = {
        "up": (0, 0, 1),
        "down": (0, 0, -1),
        "left": (-1, 0, 0),
        "right": (1, 0, 0),
        "forward": (0, 1, 0),
        "backward": (0, -1, 0)
    }
    
    if control.direction in direction_map:
        dx, dy, dz = direction_map[control.direction]
        current_pos = await system_state.get_state("robot_position")
        if isinstance(current_pos, dict):
            new_pos = current_pos.copy()
            new_pos["x"] = new_pos.get("x", 0) + dx
            new_pos["y"] = new_pos.get("y", 0) + dy
            new_pos["z"] = new_pos.get("z", 0) + dz
            await system_state.update_state("robot_position", new_pos)
            
            # Broadcast if manager available
            log_message = {
                "timestamp": datetime.now().isoformat(),
                "type": "robot",
                "message": f"机械臂移动: {control.direction}, 位置: {new_pos}"
            }
            if hasattr(system_state, '_connection_manager') and system_state._connection_manager:
                try:
                    await system_state._connection_manager.broadcast(json.dumps(log_message))
                except:
                    pass
            
            return {"status": "success", "position": new_pos}
    
    raise HTTPException(status_code=400, detail="Invalid direction")


@router.post("/mode")
async def switch_mode(
    mode_data: ModeSwitch, 
    username: str = Depends(verify_token)
):
    """Switch system mode."""
    if mode_data.mode not in ["service", "demo"]:
        raise HTTPException(status_code=400, detail="Invalid mode")
    
    await system_state.update_state("mode", mode_data.mode)
    
    # Broadcast log
    log_message = {
        "timestamp": datetime.now().isoformat(),
        "type": "system",
        "message": f"模式切换: {mode_data.mode}"
    }
    if hasattr(system_state, '_connection_manager') and system_state._connection_manager:
        try:
            await system_state._connection_manager.broadcast(json.dumps(log_message))
        except:
            pass
    
    return {"status": "success", "mode": await system_state.get_state("mode")}


@router.get("/mode")
async def get_mode(username: str = Depends(verify_token)):
    """Get current mode."""
    mode = await system_state.get_state("mode")
    return {"mode": mode}
