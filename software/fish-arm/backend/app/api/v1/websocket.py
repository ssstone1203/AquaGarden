"""
WebSocket and video streaming APIs.
"""
from fastapi import APIRouter, WebSocket, WebSocketDisconnect, HTTPException, Depends
from fastapi.responses import StreamingResponse
import asyncio
import json
import logging
import time
import cv2
import numpy as np
from datetime import datetime
from typing import List, Any

from app.core.security import verify_token
from app.services.state import system_state
from app.config import settings
from app.schemas.sensor import LogMessage

logger = logging.getLogger(__name__)

router = APIRouter(tags=["WebSocket"])


class ConnectionManager:
    """Manages active WebSocket connections."""
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self._lock = asyncio.Lock()

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        async with self._lock:
            self.active_connections.append(websocket)
        logger.info(f"WebSocket连接建立，当前连接数: {len(self.active_connections)}")

    async def disconnect(self, websocket: WebSocket):
        async with self._lock:
            if websocket in self.active_connections:
                self.active_connections.remove(websocket)
        logger.info(f"WebSocket连接断开，当前连接数: {len(self.active_connections)}")

    async def broadcast(self, message: str):
        """Broadcast message to all connected clients."""
        disconnected = []
        async with self._lock:
            for connection in self.active_connections:
                try:
                    await connection.send_text(message)
                except Exception as e:
                    logger.warning(f"发送WebSocket消息失败: {e}")
                    disconnected.append(connection)
            
            for connection in disconnected:
                if connection in self.active_connections:
                    self.active_connections.remove(connection)


# Global manager
manager = ConnectionManager()


# Set manager to state service
system_state.set_connection_manager(manager)


@router.websocket("/logs")
async def websocket_logs(websocket: WebSocket):
    """WebSocket for real-time logs and system updates."""
    await manager.connect(websocket)
    try:
        # Send initial message
        initial_message = {
            "timestamp": datetime.now().isoformat(),
            "type": "system",
            "message": "WebSocket连接已建立"
        }
        await websocket.send_text(json.dumps(initial_message))
        
        # Heartbeat loop
        last_ping = time.time()
        while True:
            current_time = time.time()
            if current_time - last_ping > settings.websocket_ping_interval:
                try:
                    await websocket.send_text(json.dumps({
                        "timestamp": datetime.now().isoformat(),
                        "type": "heartbeat",
                        "message": "ping"
                    }))
                    last_ping = current_time
                except Exception as e:
                    logger.warning(f"发送心跳失败: {e}")
                    break
            
            await asyncio.sleep(1)
            
    except WebSocketDisconnect:
        logger.info("WebSocket连接正常断开")
    except Exception as e:
        logger.error(f"WebSocket连接发生错误: {e}")
    finally:
        await manager.disconnect(websocket)


def generate_fake_video():
    """Generator for fake video stream (MJPEG)."""
    while True:
        # Create random noise image
        img = np.random.randint(0, 255, (480, 640, 3), dtype=np.uint8)
        
        # Add text overlay
        cv2.putText(img, f"Time: {datetime.now().strftime('%H:%M:%S')}", 
                    (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
        cv2.putText(img, "AquaGarden Camera Feed", 
                    (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        
        # Encode to JPEG
        ret, buffer = cv2.imencode('.jpg', img)
        if not ret:
            continue
        frame = buffer.tobytes()
        
        yield (
            b'--frame\r\n'
            b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n'
        )
        
        # Small sleep - note: in async context this needs care but for generator ok
        time.sleep(0.1)


@router.get("/video/robot")
async def video_robot(username: str = Depends(verify_token)):
    """Robot camera video stream."""
    return StreamingResponse(
        generate_fake_video(), 
        media_type="multipart/x-mixed-replace; boundary=frame"
    )


@router.get("/video/tank")
async def video_tank(username: str = Depends(verify_token)):
    """Tank camera video stream."""
    return StreamingResponse(
        generate_fake_video(), 
        media_type="multipart/x-mixed-replace; boundary=frame"
    )
