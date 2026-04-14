"""
Sensors and system data API router.
"""
from fastapi import APIRouter, Depends
import logging
from datetime import datetime

from app.core.database import get_db
from app.core.security import verify_token
from app.services.state import system_state
from app.schemas.sensor import SensorResponse, LogMessage
from app.config import settings

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/sensors", tags=["Sensors"])


@router.get("/", response_model=dict)
async def get_sensors(username: str = Depends(verify_token)):
    """Get current sensor readings with simulation."""
    # Simulate sensor update
    sensors = await system_state.simulate_sensor_update()
    return sensors


@router.get("/logs")
async def get_logs(username: str = Depends(verify_token)):
    """Get recent system logs (placeholder)."""
    # In full implementation, would query DB for logs
    return {
        "logs": [
            {
                "timestamp": datetime.now().isoformat(),
                "type": "system",
                "message": "系统初始化完成"
            }
        ],
        "total": 1
    }


@router.get("/state")
async def get_system_state(username: str = Depends(verify_token)):
    """Get full system state."""
    state = await system_state.get_state()
    return state
