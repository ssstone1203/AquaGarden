"""
API v1 Package — 包含机械臂相关路由和 Agent 对话路由
"""
from fastapi import APIRouter

from app.api.v1 import agent_api, auth, robot, websocket, agent_ws

api_router = APIRouter()

api_router.include_router(auth.router, prefix="/auth", tags=["认证"])
api_router.include_router(robot.router, prefix="/arm", tags=["机械臂"])
api_router.include_router(agent_api.router, prefix="/agent", tags=["Agent"])
api_router.include_router(websocket.router, prefix="/ws", tags=["WebSocket"])
api_router.include_router(agent_ws.router, prefix="/ws", tags=["Agent对话"])
