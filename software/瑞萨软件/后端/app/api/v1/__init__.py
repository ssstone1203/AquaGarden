"""
API v1 Package — 精简为仅包含机械臂相关路由
"""
from fastapi import APIRouter

from app.api.v1 import auth, robot, websocket

api_router = APIRouter()

api_router.include_router(auth.router, prefix="/auth", tags=["认证"])
api_router.include_router(robot.router, prefix="/arm", tags=["机械臂"])
api_router.include_router(websocket.router, prefix="/ws", tags=["WebSocket"])
