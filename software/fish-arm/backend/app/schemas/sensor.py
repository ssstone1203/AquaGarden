"""
Pydantic schemas for sensor data.
"""
from pydantic import BaseModel, Field
from typing import Optional
from datetime import datetime


class SensorData(BaseModel):
    """Sensor reading data."""
    temperature: float = Field(..., ge=0, le=100, description="温度 (°C)")
    ph: float = Field(..., ge=0, le=14, description="pH值")
    oxygen: float = Field(..., ge=0, le=20, description="溶解氧 (mg/L)")
    turbidity: float = Field(..., ge=0, le=100, description="浊度 (NTU)")


class SensorResponse(BaseModel):
    """Sensor data response."""
    temperature: float
    ph: float
    oxygen: float
    turbidity: float
    timestamp: Optional[datetime] = None


class LogMessage(BaseModel):
    """System log message."""
    timestamp: str
    type: str
    message: str
