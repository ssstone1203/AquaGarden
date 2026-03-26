"""
WebSocket 路由 — 机械臂实时状态推送
WS /api/v1/ws/arm/status
WS /api/v1/ws/arm/calibration
"""
import asyncio
from datetime import datetime, timezone

from fastapi import APIRouter, Query, WebSocket, WebSocketDisconnect

from app.core.security import decode_token
from app.schemas.robot import ArmPosition
from app.services.robot_service import robot_service
from app.utils.websocket_manager import ws_manager

router = APIRouter()


@router.websocket("/arm/status")
async def ws_arm_status(
    websocket: WebSocket,
    token: str = Query(""),
):
    """
    机械臂状态实时推送通道。

    每 2 秒主动发送一次 READ_POS 命令到 RA6M5，
    将响应 "x,y,z,pitch\\n" 解析后推送给前端。
    对应 hardware/arm/app/control/pc_control.c 中 READ_POS 命令。
    """
    await websocket.accept()

    if token:
        payload = decode_token(token)
        if payload is None:
            await websocket.close(code=4001)
            return

    ws_manager.join_robot_channel(websocket)

    try:
        while True:
            online = robot_service._online

            if online:
                # 主动查询 RA6M5 当前臂位置
                ok, resp, _ = await robot_service._serial.send("READ_POS")
                if ok:
                    try:
                        parts = resp.split(",")
                        if len(parts) >= 4:
                            pos = {
                                "x": float(parts[0]),
                                "y": float(parts[1]),
                                "z": float(parts[2]),
                                "pitch": float(parts[3]),
                            }
                            # 更新内存缓存
                            robot_service._last_pos = ArmPosition(**pos)
                        else:
                            pos = {
                                "x": robot_service._last_pos.x,
                                "y": robot_service._last_pos.y,
                                "z": robot_service._last_pos.z,
                                "pitch": robot_service._last_pos.pitch,
                            }
                    except (ValueError, IndexError):
                        pos = {
                            "x": robot_service._last_pos.x,
                            "y": robot_service._last_pos.y,
                            "z": robot_service._last_pos.z,
                            "pitch": robot_service._last_pos.pitch,
                        }
                else:
                    online = False
                    pos = {
                        "x": robot_service._last_pos.x,
                        "y": robot_service._last_pos.y,
                        "z": robot_service._last_pos.z,
                        "pitch": robot_service._last_pos.pitch,
                    }
            else:
                pos = {
                    "x": robot_service._last_pos.x,
                    "y": robot_service._last_pos.y,
                    "z": robot_service._last_pos.z,
                    "pitch": robot_service._last_pos.pitch,
                }

            await websocket.send_json({
                "type": "arm_status",
                "online": online,
                "position": pos,
                "gripper_open": robot_service._gripper_open,
                "calibration_loaded": robot_service._calibration is not None,
                "calibration_name": (
                    robot_service._calibration.name
                    if robot_service._calibration else None
                ),
                "timestamp": datetime.now(timezone.utc).isoformat(),
            })

            await asyncio.sleep(2)

    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)
    except Exception:
        ws_manager.disconnect(websocket)


@router.websocket("/arm/calibration")
async def ws_arm_calibration(
    websocket: WebSocket,
    token: str = Query(""),
):
    """标定数据实时推送通道（标定过程实时同步到前端）"""
    await websocket.accept()

    if token:
        payload = decode_token(token)
        if payload is None:
            await websocket.close(code=4001)
            return

    try:
        while True:
            data = await websocket.receive_json()
            msg_type = data.get("type")

            if msg_type == "ping":
                await websocket.send_json({
                    "type": "pong",
                    "calibration_loaded": robot_service._calibration is not None,
                    "timestamp": datetime.now(timezone.utc).isoformat(),
                })

    except WebSocketDisconnect:
        pass
    except Exception:
        pass
