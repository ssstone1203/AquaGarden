"""API v1 — 仅 Agent WebSocket。"""

from fastapi import APIRouter

from app.api.v1 import agent_ws

api_router = APIRouter()
api_router.include_router(agent_ws.router, prefix="/ws", tags=["Agent"])
