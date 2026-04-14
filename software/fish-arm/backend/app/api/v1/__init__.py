"""
API v1 routers for AquaGarden.
"""
from fastapi import APIRouter

from app.api.v1 import auth, sensors, robot, websocket

api_router = APIRouter()

# Include all routers
api_router.include_router(auth.router)
api_router.include_router(sensors.router)
api_router.include_router(robot.router)
api_router.include_router(websocket.router, prefix="/ws")
