"""
System state management service for AquaGarden.
"""
import asyncio
from datetime import datetime
from typing import Any, Dict, Optional
import logging
import random
import json

from app.config import settings
from app.schemas.sensor import LogMessage

logger = logging.getLogger(__name__)


class SystemState:
    """Manages the global system state with thread safety."""
    
    def __init__(self):
        self._state = {
            "mode": "demo",
            "robot_position": {"x": 0, "y": 0, "z": 0},
            "sensors": {
                "temperature": 25.0,
                "ph": 7.0,
                "oxygen": 8.0,
                "turbidity": 10.0
            },
            "last_update": datetime.utcnow().isoformat()
        }
        self._lock = asyncio.Lock()
        self._connection_manager = None  # Will be set from websocket
    
    async def get_state(self, key: Optional[str] = None) -> Dict[str, Any]:
        """Get current system state."""
        async with self._lock:
            if key:
                return self._state.get(key)
            return self._state.copy()
    
    async def update_state(self, key: str, value: Any):
        """Update specific state key."""
        async with self._lock:
            self._state[key] = value
            self._state["last_update"] = datetime.utcnow().isoformat()
    
    async def update_sensor_data(self, sensor_data: Dict[str, float]):
        """Update sensor readings with some simulation."""
        async with self._lock:
            self._state["sensors"].update(sensor_data)
            self._state["last_update"] = datetime.utcnow().isoformat()
    
    async def simulate_sensor_update(self) -> Dict[str, float]:
        """Simulate sensor data fluctuation."""
        sensors = self._state["sensors"]
        sensors["temperature"] = round(25.0 + random.uniform(-0.5, 0.5), 2)
        sensors["ph"] = round(7.0 + random.uniform(-0.2, 0.2), 2)
        sensors["oxygen"] = round(8.0 + random.uniform(-0.3, 0.3), 2)
        sensors["turbidity"] = round(10.0 + random.uniform(-1.0, 1.0), 2)
        self._state["last_update"] = datetime.utcnow().isoformat()
        return sensors.copy()
    
    def set_connection_manager(self, manager):
        """Set the WebSocket connection manager for broadcasting."""
        self._connection_manager = manager


# Global instance
system_state = SystemState()
