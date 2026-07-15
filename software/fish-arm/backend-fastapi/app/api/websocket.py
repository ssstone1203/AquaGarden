from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from app.core.config import settings
from app.services.logs import hub, sensor_message
from app.services.state import now_ms, state


router = APIRouter()


@router.websocket("/ws/logs")
@router.websocket("/ws/sensors")
async def logs(websocket: WebSocket) -> None:
    await hub.connect(websocket)
    fresh = state.has_fresh_hardware_snapshot(settings.sensors_realtime_max_age_ms)
    snapshot = state.read_fresh_or_demo(settings.sensors_realtime_max_age_ms)
    timestamp_ms = state.latest_real_ts if fresh else now_ms()
    source = "hardware" if fresh else "demo"
    details = state.read_sensor_details() if fresh else None
    await websocket.send_json(sensor_message(snapshot, timestamp_ms, source=source, details=details))
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        hub.disconnect(websocket)
