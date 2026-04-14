"""
Pydantic schemas for robot control.
"""
from pydantic import BaseModel, Field, validator
from typing import List, Dict, Any


class RobotControl(BaseModel):
    """Robot movement control request."""
    direction: str = Field(..., description="方向: up, down, left, right, forward, backward")
    
    @validator('direction')
    def validate_direction(cls, v):
        valid_directions = ['up', 'down', 'left', 'right', 'forward', 'backward']
        if v not in valid_directions:
            raise ValueError(f'方向必须是: {", ".join(valid_directions)}')
        return v


class ModeSwitch(BaseModel):
    """System mode switch request."""
    mode: str = Field(..., description="模式: service, demo")
    
    @validator('mode')
    def validate_mode(cls, v):
        valid_modes = ['service', 'demo']
        if v not in valid_modes:
            raise ValueError(f'模式必须是: {", ".join(valid_modes)}')
        return v


class SystemStateResponse(BaseModel):
    """System state response."""
    mode: str
    robot_position: Dict[str, float]
    sensors: Dict[str, float]
    last_update: str
