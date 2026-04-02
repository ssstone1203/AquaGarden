"""
Agent WebSocket 路由 —— 实时对话通道

WS /api/v1/ws/agent/chat
"""

import asyncio
import json
import uuid
from datetime import datetime, timezone

from fastapi import APIRouter, Query, WebSocket, WebSocketDisconnect

from app.core.security import decode_token
from app.services.agent_service import agent_service

router = APIRouter()


@router.websocket("/agent/chat")
async def ws_agent_chat(
    websocket: WebSocket,
    token: str = Query(""),
):
    """
    Agent 实时对话 WebSocket 通道。

    客户端连接后，进入等待用户输入状态。
    
    客户端发送消息格式：
    {
        "type": "message",           // 消息类型
        "text": "用户说的话",         // 文本内容
        "image_data": "data:...",    // 可选，base64 图片
    }

    服务器推送格式：
    {
        "type": "response",          // AI 响应
        "response": "AI说的话",
        "task_triggered": true,      // 是否触发了任务
        "task_name": "clamp",        // 任务名
        "session_state": "executing",
        "should_end": false,
    }
    {
        "type": "task_start",        // 任务开始
        "task": "clamp",
        "params": {},
    }
    {
        "type": "task_log",          // 任务日志
        "message": "正在识别颜色...",
    }
    {
        "type": "task_progress",     // 任务进度
        "task": "clamp",
        "step": "detecting",
        "message": "...",
    }
    {
        "type": "task_complete",      // 任务完成
        "task": "clamp",
        "result": {"success": true, "message": "..."},
    }
    {
        "type": "task_error",         // 任务错误
        "task": "clamp",
        "error": "错误信息",
    }
    """
    await websocket.accept()

    # 验证 Token
    if token:
        payload = decode_token(token)
        if payload is None:
            await websocket.close(code=4001)
            return
        user_id = payload.get("sub", "anonymous")
    else:
        user_id = "anonymous"

    # 创建会话 ID
    session_id = f"{user_id}_{uuid.uuid4().hex[:8]}"

    async def send_json(data: dict):
        """发送 JSON 数据到客户端"""
        try:
            await websocket.send_json(data)
        except Exception:
            pass

    # 注册 WebSocket 回调
    agent_service.register_ws_callback(session_id, send_json)

    # 发送连接成功消息
    await send_json({
        "type": "connected",
        "session_id": session_id,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    })

    try:
        while True:
            # 接收客户端消息
            data = await websocket.receive_json()
            msg_type = data.get("type", "")

            if msg_type == "message":
                # 处理用户消息
                text = data.get("text", "")
                image_data = data.get("image_data")

                # 调用 Agent 服务处理
                result = await agent_service.handle_message(
                    session_id=session_id,
                    text=text,
                    image_data=image_data,
                )

                # 发送 AI 响应
                await send_json({
                    "type": "response",
                    **result,
                    "timestamp": datetime.now(timezone.utc).isoformat(),
                })

            elif msg_type == "ping":
                # 心跳
                await send_json({
                    "type": "pong",
                    "timestamp": datetime.now(timezone.utc).isoformat(),
                })

            elif msg_type == "end_session":
                # 主动结束会话
                agent_service.end_session(session_id)
                await send_json({
                    "type": "session_ended",
                    "timestamp": datetime.now(timezone.utc).isoformat(),
                })

    except WebSocketDisconnect:
        pass
    except Exception:
        pass
    finally:
        # 清理
        agent_service.unregister_ws_callback(session_id)
        agent_service.end_session(session_id)
