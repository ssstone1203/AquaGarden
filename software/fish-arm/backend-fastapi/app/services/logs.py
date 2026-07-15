import asyncio
from collections import defaultdict
from typing import Any, Literal

from fastapi import WebSocket

from app.schemas.common import SensorSnapshot


class WebSocketHub:
    def __init__(self) -> None:
        self._clients: set[WebSocket] = set()
        self._loop: asyncio.AbstractEventLoop | None = None

    def bind_loop(self, loop: asyncio.AbstractEventLoop) -> None:
        self._loop = loop

    def unbind_loop(self) -> None:
        self._loop = None

    async def connect(self, websocket: WebSocket) -> None:
        await websocket.accept()
        self._clients.add(websocket)

    def disconnect(self, websocket: WebSocket) -> None:
        self._clients.discard(websocket)

    async def broadcast(self, payload: dict) -> None:
        stale: list[WebSocket] = []
        for websocket in list(self._clients):
            try:
                await websocket.send_json(payload)
            except Exception:
                stale.append(websocket)
        for websocket in stale:
            self.disconnect(websocket)

    def broadcast_from_thread(self, payload: dict) -> bool:
        loop = self._loop
        if loop is None or loop.is_closed():
            return False
        try:
            loop.call_soon_threadsafe(self._schedule_broadcast, dict(payload))
        except RuntimeError:
            return False
        return True

    def _schedule_broadcast(self, payload: dict) -> None:
        asyncio.create_task(self.broadcast(payload))


def sensor_message(
    snapshot: SensorSnapshot,
    timestamp_ms: int,
    source: Literal["hardware", "demo"] = "hardware",
    details: dict[str, Any] | None = None,
) -> dict:
    data = snapshot.model_dump()
    if details:
        data.update(details)
    return {
        "type": "sensor_data",
        "source": source,
        "ts": timestamp_ms,
        **data,
        "data": data,
    }


hub = WebSocketHub()
log_counts = defaultdict(int)
