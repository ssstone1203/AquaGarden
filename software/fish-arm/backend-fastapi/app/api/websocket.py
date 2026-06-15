from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from app.services.logs import hub


router = APIRouter()


@router.websocket("/ws/logs")
async def logs(websocket: WebSocket) -> None:
    await hub.connect(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        hub.disconnect(websocket)
