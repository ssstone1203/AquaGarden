"""
WebSocket 连接管理器
维护所有 WebSocket 连接的全局注册表，支持按频道广播
"""
import asyncio
import json
from datetime import datetime, timezone
from typing import Any

from fastapi import WebSocket, WebSocketDisconnect

from app.core.security import decode_token


class ConnectionManager:
    """
    WebSocket 连接管理器（单例模式）
    支持按任务频道隔离广播
    """

    _instance: "ConnectionManager | None" = None
    _lock: asyncio.Lock = asyncio.Lock()

    def __new__(cls) -> "ConnectionManager":
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._initialized = False
        return cls._instance

    def __init__(self) -> None:
        if self._initialized:
            return
        self._initialized = True

        # 普通连接池（全局广播）
        self._connections: list[WebSocket] = []

        # 任务频道连接池（task_id -> list[WebSocket]）
        self._task_connections: dict[int, list[WebSocket]] = {}

        # 传感器连接池
        self._sensor_connections: list[WebSocket] = []

        # 告警连接池
        self._alert_connections: list[WebSocket] = []

        # 机械臂状态连接池
        self._robot_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket) -> str | None:
        """接受 WebSocket 连接，返回 user_id 或 None"""
        await websocket.accept()

        # 尝试从 Token 解析 user_id
        try:
            # FastAPI WebSocket 不自动解析 query string，需要手动处理
            params = dict(websocket.query_params)
            token = params.get("token", "")
            if token:
                payload = decode_token(token)
                if payload:
                    user_id = payload.get("sub", "")
                    return user_id
        except Exception:
            pass

        return None

    def disconnect(self, websocket: WebSocket) -> None:
        """从所有连接池移除"""
        if websocket in self._connections:
            self._connections.remove(websocket)
        if websocket in self._sensor_connections:
            self._sensor_connections.remove(websocket)
        if websocket in self._alert_connections:
            self._alert_connections.remove(websocket)
        if websocket in self._robot_connections:
            self._robot_connections.remove(websocket)
        for task_id in list(self._task_connections.keys()):
            if websocket in self._task_connections[task_id]:
                self._task_connections[task_id].remove(websocket)

    def join_task_channel(self, websocket: WebSocket, task_id: int) -> None:
        """将连接加入指定任务频道"""
        if task_id not in self._task_connections:
            self._task_connections[task_id] = []
        if websocket not in self._task_connections[task_id]:
            self._task_connections[task_id].append(websocket)

    def leave_task_channel(self, websocket: WebSocket, task_id: int) -> None:
        """将连接移出指定任务频道"""
        if task_id in self._task_connections:
            if websocket in self._task_connections[task_id]:
                self._task_connections[task_id].remove(websocket)

    def join_sensor_channel(self, websocket: WebSocket) -> None:
        if websocket not in self._sensor_connections:
            self._sensor_connections.append(websocket)

    def join_alert_channel(self, websocket: WebSocket) -> None:
        if websocket not in self._alert_connections:
            self._alert_connections.append(websocket)

    def join_robot_channel(self, websocket: WebSocket) -> None:
        if websocket not in self._robot_connections:
            self._robot_connections.append(websocket)

    @staticmethod
    async def _safe_send(websocket: WebSocket, data: dict) -> bool:
        """安全发送 JSON，失败返回 False"""
        try:
            await websocket.send_json(data)
            return True
        except Exception:
            return False

    async def broadcast(self, message: dict) -> None:
        """广播到所有普通连接"""
        disconnected = []
        for ws in self._connections:
            if not await self._safe_send(ws, message):
                disconnected.append(ws)
        for ws in disconnected:
            self.disconnect(ws)

    async def broadcast_to_task(self, task_id: int, message: dict) -> None:
        """广播到指定任务频道"""
        if task_id not in self._task_connections:
            return
        disconnected = []
        for ws in self._task_connections[task_id]:
            if not await self._safe_send(ws, message):
                disconnected.append(ws)
        for ws in disconnected:
            self.leave_task_channel(ws, task_id)

    async def broadcast_to_sensors(self, message: dict) -> None:
        """广播到传感器频道"""
        disconnected = []
        for ws in self._sensor_connections:
            if not await self._safe_send(ws, message):
                disconnected.append(ws)
        for ws in disconnected:
            self.disconnect(ws)

    async def broadcast_to_alerts(self, message: dict) -> None:
        """广播到告警频道"""
        disconnected = []
        for ws in self._alert_connections:
            if not await self._safe_send(ws, message):
                disconnected.append(ws)
        for ws in disconnected:
            self.disconnect(ws)

    async def broadcast_to_robot(self, message: dict) -> None:
        """广播到机械臂频道"""
        disconnected = []
        for ws in self._robot_connections:
            if not await self._safe_send(ws, message):
                disconnected.append(ws)
        for ws in disconnected:
            self.disconnect(ws)

    @property
    def stats(self) -> dict[str, int]:
        """获取连接统计"""
        return {
            "total": len(self._connections),
            "sensors": len(self._sensor_connections),
            "alerts": len(self._alert_connections),
            "robot": len(self._robot_connections),
            "tasks": {tid: len(conns) for tid, conns in self._task_connections.items()},
        }


# 全局单例
ws_manager = ConnectionManager()
