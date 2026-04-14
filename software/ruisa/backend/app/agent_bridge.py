"""
Web 端桥接 agent/agent.py 的持续对话 + 任务分发给 agent/tasks.py。

- 意图解析、告别、自然回复：复用 agent.dialogue（与 agent.py 一致）
- 任务执行：在同进程内调用 tasks.task_*，与 agent.py 的 _dispatch 对齐（无语音追问时用 user_text 兜底）
"""

from __future__ import annotations

import asyncio
import io
import logging
import threading
import uuid
from contextlib import redirect_stdout
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Callable, Coroutine, Optional

from app.config import settings

logger = logging.getLogger(__name__)

_BACKEND_ROOT = Path(__file__).resolve().parent.parent
_AGENT_DIR = str(_BACKEND_ROOT / "agent")

_CAMERA_TASKS = frozenset({"clamp", "face", "answer"})

_arm = None
_arm_lock = threading.Lock()
_task_sem = asyncio.Semaphore(1)  # 全局串行任务，避免串口/camera 冲突


def _insert_agent_path() -> None:
    import sys

    if _AGENT_DIR not in sys.path:
        sys.path.insert(0, _AGENT_DIR)


def _get_arm():
    """懒加载 Arm，与 agent.py 单机单连接假设一致。"""
    global _arm
    with _arm_lock:
        if _arm is not None:
            return _arm
        _insert_agent_path()
        from arm import Arm  # noqa: WPS433 — agent 包内模块

        import config as agent_config  # noqa: WPS433

        _arm = Arm(agent_config.SERIAL_PORT)
        return _arm


class SessionState(str, Enum):
    IDLE = "idle"
    WAITING_TASK = "waiting_task"
    EXECUTING = "executing"
    DIALOGUE = "dialogue"
    ENDED = "ended"


@dataclass
class DialogueMessage:
    role: str
    content: str
    timestamp: datetime = field(default_factory=datetime.now)


@dataclass
class AgentSession:
    session_id: str
    state: SessionState = SessionState.IDLE
    messages: list[DialogueMessage] = field(default_factory=list)
    debug_keyboard: bool = False
    awake: bool = True


def _dispatch_tasks(arm, task_name: str, params: dict, user_text: str) -> None:
    """
    与 agent/agent.py _dispatch 等价；Web 端不调用 dialogue.listen 追问，
    answer / action 缺参时用 user_text 或列表兜底。
    """
    _insert_agent_path()
    import dialogue
    from tasks import (  # noqa: WPS433
        list_actions,
        task_action,
        task_answer,
        task_clamp,
        task_face,
        task_led,
    )

    if task_name == "clamp":
        task_clamp(arm)
    elif task_name == "led":
        preset = params.get("preset") or "medium"
        task_led(arm, preset_key=preset)
    elif task_name == "face":
        task_face(arm)
    elif task_name == "answer":
        q = (params.get("question") or "").strip()
        if not q:
            q = (user_text or "").strip() or "请解答图片中的题目"
        task_answer(arm, question=q)
    elif task_name == "action":
        action_name = params.get("action", "") or ""
        available = list_actions()
        if action_name not in available:
            action_name = ""
        if not action_name and len(available) == 1:
            action_name = available[0]
        if action_name:
            task_action(arm, action_name)
        else:
            dialogue.speak("没有找到对应的动作，请重新尝试。")
    else:
        dialogue.speak("抱歉，没有理解你的意思，请再说一次。")


def _run_task_blocking(task_name: str, params: dict, user_text: str) -> tuple[bool, str, Optional[str]]:
    """在后台线程中执行任务，捕获 stdout 作为日志。"""
    buf = io.StringIO()
    err: Optional[str] = None
    try:
        arm = _get_arm()
        with redirect_stdout(buf):
            _dispatch_tasks(arm, task_name, params, user_text)
    except Exception as e:
        logger.exception("task failed")
        err = str(e)
    return err is None, buf.getvalue(), err


class AgentBridge:
    """管理 WebSocket 会话；与 agent.py 对话状态机对齐。"""

    AGENT_GREETING = "你好呀！我是小臂，你的学习小助手~ 有什么需要帮忙的吗？"

    def __init__(self):
        self._sessions: dict[str, AgentSession] = {}
        self._ws_callbacks: dict[str, Callable[[dict], Coroutine[Any, Any, None]]] = {}

    async def _speak_assistant_async(self, text: Optional[str]) -> None:
        """与 agent.py 一致：DashScope TTS（config.TTS_MODEL / TTS_VOICE）+ pygame，跑在线程池避免阻塞事件循环。"""
        if not text or not str(text).strip():
            return
        try:

            def _run():
                _insert_agent_path()
                import dialogue  # noqa: WPS433

                dialogue.speak(str(text).strip())

            await asyncio.to_thread(_run)
        except Exception as e:
            logger.warning("assistant TTS failed: %s", e)

    def register_ws_callback(self, session_id: str, callback: Callable[[dict], Coroutine[Any, Any, None]]):
        self._ws_callbacks[session_id] = callback

    def unregister_ws_callback(self, session_id: str):
        self._ws_callbacks.pop(session_id, None)

    async def send_to_client(self, session_id: str, data: dict):
        cb = self._ws_callbacks.get(session_id)
        if cb:
            try:
                await cb(data)
            except Exception as e:
                logger.error("ws send failed: %s", e)

    def ensure_session(self, session_id: str) -> AgentSession:
        if session_id in self._sessions:
            return self._sessions[session_id]
        dk = settings.agent_debug_keyboard
        s = AgentSession(
            session_id=session_id,
            debug_keyboard=dk,
            awake=not dk,
            state=SessionState.IDLE if dk else SessionState.WAITING_TASK,
        )
        self._sessions[session_id] = s
        return s

    def get_session(self, session_id: str) -> Optional[AgentSession]:
        return self._sessions.get(session_id)

    def set_debug_keyboard(self, session_id: str, enabled: bool) -> AgentSession:
        s = self.ensure_session(session_id)
        prev = s.debug_keyboard
        s.debug_keyboard = bool(enabled)
        if s.debug_keyboard:
            if not prev:
                s.awake = False
                s.state = SessionState.IDLE
        else:
            s.awake = True
            if s.state in (SessionState.IDLE, SessionState.ENDED):
                s.state = SessionState.WAITING_TASK
        return s

    def end_session(self, session_id: str) -> bool:
        return self._sessions.pop(session_id, None) is not None

    async def handle_debug_wake(self, session_id: str) -> dict:
        s = self.ensure_session(session_id)
        if not s.debug_keyboard:
            msg = "当前为普通模式，无需模拟唤醒，可直接发送指令。"
            asyncio.create_task(self._speak_assistant_async(msg))
            return self._ok_response(
                msg,
                s,
                task_triggered=False,
                needs_wake=False,
            )
        s.awake = True
        if s.state == SessionState.IDLE:
            s.state = SessionState.WAITING_TASK
        s.messages.append(DialogueMessage(role="assistant", content=self.AGENT_GREETING))
        asyncio.create_task(self._speak_assistant_async(self.AGENT_GREETING))
        return self._ok_response(self.AGENT_GREETING, s, task_triggered=False, needs_wake=False)

    def _ok_response(
        self,
        response: str,
        s: AgentSession,
        *,
        task_triggered: bool,
        task_name: Optional[str] = None,
        task_params: Optional[dict] = None,
        should_end: bool = False,
        needs_wake: Optional[bool] = None,
    ) -> dict:
        return {
            "response": response,
            "task_triggered": task_triggered,
            "task_name": task_name,
            "task_params": task_params or {},
            "session_state": s.state.value,
            "should_end": should_end,
            "needs_wake": needs_wake if needs_wake is not None else (s.debug_keyboard and not s.awake),
        }

    async def handle_message(
        self,
        session_id: str,
        text: str,
        image_data: Optional[str] = None,
    ) -> dict:
        _ = image_data  # agent.py / tasks 以摄像头为准，Web 端暂不传图进 task_answer
        s = self.ensure_session(session_id)

        if s.debug_keyboard and not s.awake:
            msg = "【DEBUG】尚未唤醒。请点击「模拟唤醒」或发送 debug_wake（等同 DEBUG=1 运行 agent.py 后按 Enter）。"
            asyncio.create_task(self._speak_assistant_async(msg))
            return self._ok_response(
                msg,
                s,
                task_triggered=False,
                needs_wake=True,
            )

        s.messages.append(DialogueMessage(role="user", content=text))
        _insert_agent_path()
        import dialogue  # noqa: WPS433 — agent/ 目录内模块

        if dialogue.should_end_session(text):
            response = dialogue.generate_farewell_response()
            s.state = SessionState.IDLE
            s.awake = not s.debug_keyboard
            s.messages.append(DialogueMessage(role="assistant", content=response))

            async def go_home_after_farewell():
                async with _task_sem:

                    def _home():
                        _insert_agent_path()
                        try:
                            arm = _get_arm()
                            arm.go_home()
                        except Exception as e:
                            logger.warning("go_home: %s", e)

                    await asyncio.to_thread(_home)

            asyncio.create_task(go_home_after_farewell())

            asyncio.create_task(self._speak_assistant_async(response))
            return self._ok_response(
                response,
                s,
                task_triggered=False,
                should_end=True,
                needs_wake=s.debug_keyboard and not s.awake,
            )

        if s.state == SessionState.IDLE:
            s.state = SessionState.WAITING_TASK

        task_name, task_params = self._parse_intent(text)

        if task_name and task_name != "unknown":
            s.state = SessionState.EXECUTING
            response = dialogue.generate_natural_response(task_name, task_params or {})
            s.messages.append(DialogueMessage(role="assistant", content=response))
            asyncio.create_task(self._speak_assistant_async(response))
            asyncio.create_task(
                self._execute_task_async(session_id, task_name, task_params or {}, text)
            )
            return self._ok_response(
                response,
                s,
                task_triggered=True,
                task_name=task_name,
                task_params=task_params or {},
            )

        s.state = SessionState.DIALOGUE
        response = "嗯嗯，我听到了~ 你还有什么想让我帮忙的吗？"
        s.messages.append(DialogueMessage(role="assistant", content=response))
        asyncio.create_task(self._speak_assistant_async(response))
        return self._ok_response(response, s, task_triggered=False)

    def _parse_intent(self, text: str) -> tuple[str, dict]:
        _insert_agent_path()
        import dialogue  # noqa: WPS433

        try:
            return dialogue.parse(text)
        except Exception as e:
            logger.warning("parse failed: %s", e)
            return "unknown", {}

    async def _execute_task_async(
        self,
        session_id: str,
        task_name: str,
        params: dict,
        user_text: str,
    ):
        use_cam = task_name in _CAMERA_TASKS
        await self.send_to_client(session_id, {"type": "task_start", "task": task_name, "params": params})
        if use_cam:
            await self.send_to_client(
                session_id,
                {
                    "type": "camera_preview",
                    "show": True,
                    "mjpeg_path": "/api/v1/camera/mjpeg",
                },
            )
        agent_tasks_mod = None
        try:
            async with _task_sem:
                ok, stdout_text, err = await asyncio.to_thread(
                    _run_task_blocking, task_name, params, user_text
                )
            _insert_agent_path()
            import tasks as agent_tasks_mod  # noqa: WPS433

            for line in stdout_text.splitlines():
                line = line.strip()
                if line:
                    await self.send_to_client(session_id, {"type": "task_log", "message": line})

            result: dict[str, Any] = {
                "success": True,
                "message": "任务流程已结束（详见日志）。",
            }
            if ok and task_name == "answer":
                ans = getattr(agent_tasks_mod, "last_answer_for_web", None)
                if ans:
                    result["answer"] = ans

            if ok:
                await self.send_to_client(
                    session_id,
                    {"type": "task_complete", "task": task_name, "result": result},
                )
            else:
                await self.send_to_client(
                    session_id,
                    {"type": "task_error", "task": task_name, "error": err or "unknown"},
                )
        finally:
            if agent_tasks_mod is not None:
                try:
                    agent_tasks_mod.last_answer_for_web = None
                except Exception:
                    pass
            if use_cam:
                _insert_agent_path()
                try:
                    import camera_preview  # noqa: WPS433

                    camera_preview.clear()
                except Exception:
                    pass
                await self.send_to_client(
                    session_id,
                    {"type": "camera_preview", "show": False},
                )
            sess = self.get_session(session_id)
            if sess:
                sess.state = SessionState.WAITING_TASK


agent_bridge = AgentBridge()


def new_session_id() -> str:
    return f"web_{uuid.uuid4().hex[:12]}"
