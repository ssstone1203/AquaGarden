"""Agent WebSocket — 与 agent_bridge 对接（无登录）。"""

from datetime import datetime, timezone

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from app.agent_bridge import agent_bridge, new_session_id

router = APIRouter()


@router.websocket("/agent/chat")
async def ws_agent_chat(websocket: WebSocket):
    await websocket.accept()
    session_id = new_session_id()

    async def send_json(data: dict):
        try:
            await websocket.send_json(data)
        except Exception:
            pass

    agent_bridge.register_ws_callback(session_id, send_json)
    sess = agent_bridge.ensure_session(session_id)

    await send_json(
        {
            "type": "connected",
            "session_id": session_id,
            "agent_debug_keyboard": sess.debug_keyboard,
            "needs_wake": sess.debug_keyboard and not sess.awake,
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }
    )

    try:
        while True:
            data = await websocket.receive_json()
            msg_type = data.get("type", "")

            if msg_type == "message":
                text = data.get("text", "")
                image_data = data.get("image_data")
                result = await agent_bridge.handle_message(
                    session_id=session_id,
                    text=text,
                    image_data=image_data,
                )
                await send_json(
                    {
                        "type": "response",
                        **result,
                        "timestamp": datetime.now(timezone.utc).isoformat(),
                    }
                )

            elif msg_type == "ping":
                await send_json({"type": "pong", "timestamp": datetime.now(timezone.utc).isoformat()})

            elif msg_type == "debug_wake":
                result = await agent_bridge.handle_debug_wake(session_id)
                await send_json(
                    {
                        "type": "response",
                        **result,
                        "timestamp": datetime.now(timezone.utc).isoformat(),
                    }
                )

            elif msg_type == "set_debug_keyboard":
                enabled = bool(data.get("enabled", False))
                s = agent_bridge.set_debug_keyboard(session_id, enabled)
                await send_json(
                    {
                        "type": "debug_mode",
                        "agent_debug_keyboard": s.debug_keyboard,
                        "needs_wake": s.debug_keyboard and not s.awake,
                        "timestamp": datetime.now(timezone.utc).isoformat(),
                    }
                )

            elif msg_type == "end_session":
                agent_bridge.end_session(session_id)
                await send_json({"type": "session_ended", "timestamp": datetime.now(timezone.utc).isoformat()})

    except WebSocketDisconnect:
        pass
    finally:
        agent_bridge.unregister_ws_callback(session_id)
        agent_bridge.end_session(session_id)
