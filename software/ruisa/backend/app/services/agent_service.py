"""
agent_service.py —— Agent 对话服务（与 agent/agent.py 五项任务一一对应）

任务表（与 agent.py 文档一致）：
  1. clamp  —— 颜色识别与分拣  → task_clamp / 后端 run_clamp_task
  2. led    —— 智能台灯        → task_led
  3. face   —— 人脸识别追踪    → task_face
  4. answer —— 题目解答        → task_answer
  5. action —— 动作执行        → task_action（JSON 关键帧）

管理 AI 对话会话、任务执行和 WebSocket 实时通信。
"""

import asyncio
import json
import logging
import time
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from types import ModuleType
from typing import Any, Callable, Optional

from app.config import settings
from app.services.robot_service import robot_service

logger = logging.getLogger(__name__)

_RUISA_ROOT = Path(__file__).resolve().parent.parent.parent.parent


class SessionState(str, Enum):
    """会话状态"""
    IDLE = "idle"                    # 空闲，等待唤醒
    WAITING_TASK = "waiting_task"    # 已唤醒，等待用户任务
    EXECUTING = "executing"          # 执行任务中
    DIALOGUE = "dialogue"             # 自由对话中
    ENDED = "ended"                  # 会话结束


@dataclass
class DialogueMessage:
    """对话消息"""
    role: str           # "user" | "assistant" | "system"
    content: str
    timestamp: datetime = field(default_factory=datetime.now)


@dataclass
class AgentSession:
    """Agent 会话"""
    session_id: str
    state: SessionState = SessionState.IDLE
    messages: list[DialogueMessage] = field(default_factory=list)
    current_task: Optional[str] = None
    task_params: dict = field(default_factory=dict)
    created_at: datetime = field(default_factory=datetime.now)
    # 键盘 DEBUG：与 AGENT_DEBUG_KEYBOARD / 前端开关一致；为 True 时须先 handle_debug_wake
    debug_keyboard: bool = False
    awake: bool = True


class AgentService:
    """
    Agent 服务：管理多用户会话、对话历史和任务执行。
    
    核心流程：
    1. 用户发起会话 → 创建 AgentSession
    2. 持续对话（不退出）直到用户说"拜拜"
    3. 支持意图识别 → 执行任务 → 返回结果 → 继续对话
    """

    def __init__(self):
        # session_id → AgentSession
        self._sessions: dict[str, AgentSession] = {}
        # WebSocket 回调
        self._ws_callbacks: dict[str, Callable] = {}
        # agent/pyc_loader：可选 .pyc 扩展（AGENT_TRAINED_PYC）
        self._trained_module: Optional[ModuleType] = None
        self._trained_load_attempted: bool = False
        self._trained_resolved_path: Optional[Path] = None

    # ── 会话管理 ────────────────────────────────────────────────────────────

    def create_session(self, session_id: str) -> AgentSession:
        """创建新会话"""
        dk = settings.agent_debug_keyboard
        session = AgentSession(
            session_id=session_id,
            debug_keyboard=dk,
            awake=not dk,
        )
        self._sessions[session_id] = session
        logger.info(
            f"[Agent] 创建会话: {session_id} (debug_keyboard={session.debug_keyboard}, awake={session.awake})"
        )
        return session

    def ensure_session(self, session_id: str) -> AgentSession:
        """获取或创建会话（WebSocket 连接建立后调用）"""
        if session_id in self._sessions:
            return self._sessions[session_id]
        return self.create_session(session_id)

    def set_debug_keyboard(self, session_id: str, enabled: bool) -> AgentSession:
        """
        切换当前会话的键盘 DEBUG 模式（前端按钮，等同可选的 AGENT_DEBUG_KEYBOARD）。
        开启：需模拟唤醒后再对话；关闭：可直接键盘输入对话。

        注意：开启 DEBUG 时仅在「刚从非 DEBUG 切入」时把 awake 置 False。
        若会话已是 DEBUG，重复的 set_debug_keyboard(true)（如本地存储与连接先后各发一次）
        不得清空已模拟唤醒状态，否则用户唤醒后下一条指令会再次被未唤醒门禁拦住。
        """
        session = self.ensure_session(session_id)
        prev_dk = session.debug_keyboard
        session.debug_keyboard = bool(enabled)

        if session.debug_keyboard:
            if not prev_dk:
                session.awake = False
                session.state = SessionState.IDLE
        else:
            session.awake = True
            if session.state in (SessionState.IDLE, SessionState.ENDED):
                session.state = SessionState.WAITING_TASK

        logger.info(
            f"[Agent] 会话 {session_id} debug_keyboard={session.debug_keyboard} awake={session.awake}"
        )
        return session

    AGENT_GREETING = "你好呀！我是小臂，你的学习小助手~ 有什么需要帮忙的吗？"

    async def handle_debug_wake(self, session_id: str) -> dict:
        """DEBUG 模式：模拟 agent.py 中按 Enter 唤醒，并播报与 _continuous_session 相同的 greeting。"""
        session = self.get_session(session_id) or self.create_session(session_id)
        if not session.debug_keyboard:
            return {
                "response": "当前为普通模式，无需模拟唤醒，可直接在输入框用键盘发送指令。",
                "task_triggered": False,
                "task_name": None,
                "task_params": {},
                "session_state": session.state.value,
                "should_end": False,
                "needs_wake": False,
            }
        session.awake = True
        if session.state == SessionState.IDLE:
            session.state = SessionState.WAITING_TASK
        session.messages.append(DialogueMessage(role="assistant", content=self.AGENT_GREETING))
        return {
            "response": self.AGENT_GREETING,
            "task_triggered": False,
            "task_name": None,
            "task_params": {},
            "session_state": session.state.value,
            "should_end": False,
            "needs_wake": False,
        }

    def get_session(self, session_id: str) -> Optional[AgentSession]:
        """获取会话"""
        return self._sessions.get(session_id)

    def end_session(self, session_id: str) -> bool:
        """结束会话"""
        if session_id in self._sessions:
            session = self._sessions[session_id]
            session.state = SessionState.ENDED
            logger.info(f"[Agent] 结束会话: {session_id}")
            del self._sessions[session_id]
            return True
        return False

    # ── WebSocket 回调 ──────────────────────────────────────────────────────

    def register_ws_callback(self, session_id: str, callback: Callable):
        """注册 WebSocket 发送回调"""
        self._ws_callbacks[session_id] = callback

    def unregister_ws_callback(self, session_id: str):
        """取消注册 WebSocket 回调"""
        if session_id in self._ws_callbacks:
            del self._ws_callbacks[session_id]

    async def send_to_client(self, session_id: str, data: dict):
        """通过 WebSocket 发送数据到客户端"""
        callback = self._ws_callbacks.get(session_id)
        if callback:
            try:
                await callback(data)
            except Exception as e:
                logger.error(f"[Agent] WebSocket 发送失败: {e}")

    # ── 对话接口 ────────────────────────────────────────────────────────────

    async def handle_message(
        self,
        session_id: str,
        text: str,
        image_data: Optional[str] = None,
    ) -> dict:
        """
        处理用户消息，返回 AI 响应。
        
        Args:
            session_id: 会话ID
            text: 用户输入文本
            image_data: 可选的 base64 图片数据
        
        Returns:
            {
                "response": str,           # AI 响应文本
                "task_triggered": bool,     # 是否触发了任务
                "task_name": str,         # 任务名
                "task_params": dict,       # 任务参数
                "session_state": str,      # 会话状态
                "should_end": bool,        # 是否应结束会话
            }
        """
        session = self.get_session(session_id)
        if not session:
            session = self.create_session(session_id)

        # 键盘 DEBUG：未唤醒时拒绝对话（与 agent.py 等待 Enter 一致）
        if session.debug_keyboard and not session.awake:
            return {
                "response": (
                    "【DEBUG】尚未唤醒。请点击「模拟唤醒」或发送 WebSocket `{type:\"debug_wake\"}`，"
                    "等价于本地 `DEBUG=1 python agent.py` 后按一次 Enter。"
                ),
                "task_triggered": False,
                "task_name": None,
                "task_params": {},
                "session_state": session.state.value,
                "should_end": False,
                "needs_wake": True,
            }

        # 添加用户消息
        session.messages.append(DialogueMessage(
            role="user",
            content=text,
        ))

        # 与 agent/agent.py + dialogue.should_end_session 一致
        from agent import dialogue

        if dialogue.should_end_session(text):
            response = dialogue.generate_farewell_response()
            # 不删除会话，保留 debug_keyboard；键盘 DEBUG 下需再次唤醒（与 agent.py 循环一致）
            session.state = SessionState.IDLE
            session.awake = not session.debug_keyboard
            session.messages.append(DialogueMessage(
                role="assistant",
                content=response,
            ))
            return {
                "response": response,
                "task_triggered": False,
                "task_name": None,
                "task_params": {},
                "session_state": session.state.value,
                "should_end": True,
                "needs_wake": session.debug_keyboard and not session.awake,
            }

        # 更新状态
        if session.state == SessionState.IDLE:
            session.state = SessionState.WAITING_TASK

        # 调用对话模块解析意图
        task_name, task_params = self._parse_intent(text)

        if task_name and task_name != "unknown":
            # 触发了任务
            session.current_task = task_name
            session.task_params = task_params
            session.state = SessionState.EXECUTING

            # 生成自然的任务确认
            response = self._generate_task_response(task_name, task_params)

            # 异步执行任务（不阻塞对话）；传入原句便于分拣颜色等兜底解析
            asyncio.create_task(
                self._execute_task(session_id, task_name, task_params, text)
            )

        else:
            # 与 agent/agent.py _continuous_session：unknown 时固定提示（无图）
            session.state = SessionState.DIALOGUE
            response = "嗯嗯，我听到了~ 你还有什么想让我帮忙的吗？"
            if image_data:
                # 带图时仍走多模态，便于后续扩展拍题以外的识图
                response = await self._generate_free_response(session, text, image_data)

        # 添加 AI 响应
        session.messages.append(DialogueMessage(
            role="assistant",
            content=response,
        ))

        return {
            "response": response,
            "task_triggered": task_name not in [None, "unknown"],
            "task_name": task_name if task_name != "unknown" else None,
            "task_params": task_params,
            "session_state": session.state.value,
            "should_end": False,
            "needs_wake": False,
        }

    def _resolve_agent_trained_pyc_path(self) -> Optional[Path]:
        raw = settings.agent_trained_pyc
        if raw is None:
            return None
        p = Path(raw).expanduser()
        if not p.is_absolute():
            p = (_RUISA_ROOT / p).resolve()
        else:
            p = p.resolve()
        return p

    def _ensure_trained_resolved_path(self) -> Optional[Path]:
        if self._trained_resolved_path is None:
            self._trained_resolved_path = self._resolve_agent_trained_pyc_path()
        return self._trained_resolved_path

    def get_trained_pyc_path(self) -> Optional[Path]:
        """配置中的 .pyc 绝对路径（不触发加载）。"""
        return self._ensure_trained_resolved_path()

    def _load_trained_module_if_needed(self) -> Optional[ModuleType]:
        if self._trained_load_attempted:
            return self._trained_module
        self._trained_load_attempted = True
        path = self._ensure_trained_resolved_path()
        if path is None or not path.is_file():
            return None
        from agent.pyc_loader import load_pyc_module

        self._trained_module = load_pyc_module(path)
        return self._trained_module

    def get_trained_module(self) -> Optional[ModuleType]:
        """已加载的 AGENT_TRAINED_PYC 模块（若失败则为 None）。"""
        return self._load_trained_module_if_needed()

    def _parse_intent(self, text: str) -> tuple[Optional[str], dict]:
        """
        解析用户意图。
        若 AGENT_TRAINED_PYC 提供 parse_intent(text) 且返回非 unknown，优先采用；
        否则复用 dialogue.parse，再降级关键词。
        """
        mod = self._load_trained_module_if_needed()
        if mod is not None:
            fn = getattr(mod, "parse_intent", None)
            if callable(fn):
                try:
                    out = fn(text)
                    if isinstance(out, tuple) and len(out) >= 2:
                        task, params = out[0], out[1]
                        if task not in (None, "unknown"):
                            return task, params if isinstance(params, dict) else {}
                except Exception as e:
                    logger.warning("[Agent] 扩展 parse_intent 异常，回退内置逻辑: %s", e)
        try:
            from agent.dialogue import parse

            task_n, params_n = parse(text)
        except ImportError:
            return self._keyword_parse(text)
        if task_n in (None, "unknown"):
            kw_t, kw_p = self._keyword_parse(text)
            if kw_t not in (None, "unknown"):
                return kw_t, kw_p
        return task_n, params_n

    def _resolve_clamp_color(self, params: dict, user_text: str) -> str:
        """分拣目标颜色：优先 LLM params，再从中文/英文原句推断，默认 red。"""
        c = params.get("color")
        if isinstance(c, str):
            cl = c.strip().lower()
            zh = {"红": "red", "红色": "red", "绿": "green", "绿色": "green", "蓝": "blue", "蓝色": "blue"}
            if cl in zh:
                return zh[cl]
            if cl in ("red", "green", "blue"):
                return cl
        t = user_text or ""
        if "绿" in t or "green" in t.lower():
            return "green"
        if "蓝" in t or "blue" in t.lower():
            return "blue"
        if "红" in t or "red" in t.lower():
            return "red"
        return "red"

    @staticmethod
    def _agent_script_worker_preamble():
        """与 agent.py 一致：把 agent 目录插入 path，以便 tasks/arm 的本地 import。"""
        import sys

        agent_dir = str(_RUISA_ROOT / "agent")
        if agent_dir not in sys.path:
            sys.path.insert(0, agent_dir)

    def _keyword_parse(self, text: str) -> tuple[str, dict]:
        """简单的关键词匹配降级"""
        keywords_map = [
            (["颜色", "分拣", "积木", "物块", "夹取", "分类"], "clamp", {}),
            (
                [
                    "台灯", "照明", "灯", "光", "亮度",
                    "调亮", "调暗", "打开灯", "开灯", "关闭灯", "关灯",
                ],
                "led",
                self._parse_led_params(text),
            ),
            (["人脸", "追踪", "跟踪", "在不在", "看我"], "face", {}),
            (["题目", "解答", "分析", "拍照", "作业", "解题", "题"], "answer", {}),
            (["跳舞", "招手", "挥手", "点头", "摇头", "看天气", "动作"], "action", self._parse_action_params(text)),
        ]

        for keywords, task, params in keywords_map:
            if any(w in text for w in keywords):
                return task, params

        return "unknown", {}

    def _parse_led_params(self, text: str) -> dict:
        """解析台灯参数（与 agent/dialogue._parse_led_params 对齐）"""
        preset = "medium"
        if any(w in text for w in ["关闭", "熄", "关掉", "关灯", "关"]):
            preset = "off"
        elif any(w in text for w in ["调暗", "低", "暗", "暗一点"]):
            preset = "low"
        elif any(w in text for w in ["调亮", "高", "亮", "亮一点", "最亮", "打开", "开灯"]):
            preset = "high"
        return {"preset": preset}

    def _parse_action_params(self, text: str) -> dict:
        """解析动作参数"""
        action_map = {
            "跳舞": ["跳舞", "跳个舞", "dance"],
            "打招呼": ["打招呼", "招手", "挥手", "你好"],
            "点头": ["点头", "同意"],
            "摇头": ["摇头", "不"],
            "看天气": ["天气", "看天气"],
        }
        for action, keywords in action_map.items():
            if any(kw in text for kw in keywords):
                return {"action": action}
        return {"action": "打招呼"}  # 默认打招呼

    def _generate_task_response(self, task_name: str, params: dict) -> str:
        """与 agent/agent.py 一致：使用 dialogue.generate_natural_response 做任务确认语"""
        from agent import dialogue

        return dialogue.generate_natural_response(task_name, params or {})

    async def _generate_free_response(
        self,
        session: AgentSession,
        text: str,
        image_data: Optional[str],
    ) -> str:
        """生成自由对话响应（调用大模型）"""
        try:
            from agent import config as agent_config
            from openai import OpenAI

            if not agent_config.DASHSCOPE_API_KEY:
                return "抱歉，我现在没法回答这个问题。"

            client = OpenAI(
                api_key=agent_config.DASHSCOPE_API_KEY,
                base_url=agent_config.DASHSCOPE_BASE_URL,
            )

            # 构建消息历史
            messages = [
                {"role": "system", "content": self._get_system_prompt()},
            ]
            for msg in session.messages[-6:]:  # 最近6条
                messages.append({"role": msg.role, "content": msg.content})

            # 添加当前消息
            if image_data:
                messages.append({
                    "role": "user",
                    "content": [
                        {"type": "text", "text": text},
                        {"type": "image_url", "image_url": {"url": image_data}},
                    ],
                })
            else:
                messages.append({"role": "user", "content": text})

            resp = client.chat.completions.create(
                model=agent_config.LLM_MODEL,
                messages=messages,
                temperature=0.7,
                max_tokens=200,
            )
            return resp.choices[0].message.content.strip()

        except Exception as e:
            logger.error(f"[Agent] 自由对话生成失败: {e}")
            return "嗯，让我想想... 你说的这个问题我还在学习中。"

    def _get_system_prompt(self) -> str:
        """获取系统提示词（更自然的对话风格）"""
        return """你是一个可爱友好的学习助手机器人，说话要自然、亲切，像和朋友聊天一样。

要求：
1. 不要直接说"执行任务"，用自然的表达如"好的，我来帮你..."
2. 回答要简短、口语化，适合语音播报
3. 适当使用表情符号或语气词增加亲切感
4. 遇到不懂的问题，要诚实但友好地回应
5. 可以适当关心用户的学习状态

示例回复：
- "好的呀，我来帮你看看这道题！"
- "嗯嗯，我理解了，让我来帮你分拣积木！"
- "好的，我这就帮你调亮灯光！"
- "没问题，我来打个招呼！" """

    # ── 任务执行 ────────────────────────────────────────────────────────────

    async def _execute_task(
        self,
        session_id: str,
        task_name: str,
        params: dict,
        user_text: str = "",
    ):
        """异步执行任务，实时推送进度到客户端"""
        try:
            await self.send_to_client(session_id, {
                "type": "task_start",
                "task": task_name,
                "params": params,
            })

            # 根据任务类型执行（与 software/ruisa/agent 下能力对齐）
            if task_name == "clamp":
                result = await self._execute_clamp_task(
                    session_id, params, user_text,
                )
            elif task_name == "led":
                result = await self._execute_led_task(session_id, params.get("preset", "medium"))
            elif task_name == "face":
                result = await self._execute_face_task(session_id)
            elif task_name == "answer":
                result = await self._execute_answer_task(session_id, params, user_text)
            elif task_name == "action":
                result = await self._execute_action_task(session_id, params.get("action", "打招呼"))
            else:
                result = {
                    "success": False,
                    "message": "抱歉，没有理解你的意思，请再说一次。",
                }

            # 发送任务结果
            await self.send_to_client(session_id, {
                "type": "task_complete",
                "task": task_name,
                "result": result,
            })

            # 更新会话状态
            session = self.get_session(session_id)
            if session:
                session.state = SessionState.WAITING_TASK
                session.current_task = None

        except Exception as e:
            logger.error(f"[Agent] 任务执行失败: {e}")
            await self.send_to_client(session_id, {
                "type": "task_error",
                "task": task_name,
                "error": str(e),
            })

    async def _execute_clamp_task(
        self,
        session_id: str,
        params: dict,
        user_text: str,
    ) -> dict:
        """① 颜色识别与分拣 — agent task_clamp；后端用 run_clamp_task 演示流程"""
        try:
            from app.database import async_session_maker

            color = self._resolve_clamp_color(params, user_text)
            await self.send_to_client(session_id, {
                "type": "task_log",
                "message": f"开始执行分拣流程（目标颜色: {color}）...",
            })

            status = await robot_service.get_status()
            if not status.get("online"):
                return {"success": False, "message": "机械臂未连接，请先在控制台连接串口"}

            await self.send_to_client(session_id, {
                "type": "task_progress",
                "task": "clamp",
                "step": "running",
                "message": "机械臂运动中，请远离工作区...",
            })

            async with async_session_maker() as db:
                res = await robot_service.run_clamp_task(db, color, True)

            msg = (
                "颜色分拣完成！"
                if res.success
                else (res.steps[-1] if res.steps else "分拣未成功")
            )
            return {
                "success": res.success,
                "message": msg,
                "color": res.color,
                "steps": res.steps,
                "duration_seconds": res.duration_seconds,
            }

        except Exception as e:
            logger.error(f"[Agent] 分拣任务失败: {e}")
            return {"success": False, "message": f"分拣失败: {str(e)}"}

    async def _execute_led_task(self, session_id: str, preset: str) -> dict:
        """
        执行台灯控制任务。
        串口协议须与 hardware/arm/app/control/pc_control.c 及 agent/arm.py 一致：
        LED_BRIGHT n → LED_ALL r g b（不可使用单条 LED r g b bright，固件不识别）。
        """
        try:
            from agent import config as agent_config
            from app.database import async_session_maker

            presets = agent_config.LED_PRESETS
            preset_data = presets.get(preset, presets.get("medium"))

            if not await robot_service.is_online():
                return {"success": False, "message": "机械臂串口未连接，无法控制灯光"}

            async def send_led_strip(r: int, g: int, b: int, bright: int) -> tuple[bool, str]:
                ok_b, rb, _ = await robot_service._serial.send(f"LED_BRIGHT {int(bright)}")
                ok_c, rc, _ = await robot_service._serial.send(
                    f"LED_ALL {int(r)} {int(g)} {int(b)}"
                )
                return (ok_b and ok_c), f"{rb}|{rc}"

            if preset == "off":
                await self.send_to_client(session_id, {
                    "type": "task_log",
                    "message": "正在关闭台灯...",
                })
                ok, resp = await send_led_strip(0, 0, 0, 0)
            else:
                await self.send_to_client(session_id, {
                    "type": "task_log",
                    "message": f"正移动到台灯位并调节为{preset}模式...",
                })
                async with async_session_maker() as db:
                    await robot_service.send_command(db, "MOVE", {
                        "x": agent_config.LED_X,
                        "y": agent_config.LED_Y,
                        "z": agent_config.LED_Z,
                        "pitch": agent_config.LED_PITCH,
                        "min_pitch": -90.0,
                        "max_pitch": 90.0,
                        "duration": 2000,
                    })
                ok, resp = await send_led_strip(
                    preset_data["r"],
                    preset_data["g"],
                    preset_data["b"],
                    preset_data["bright"],
                )

            if ok:
                return {"success": True, "message": "灯光调节完成！"}
            logger.warning("[Agent] 台灯串口响应异常: %s", resp)
            return {"success": False, "message": f"灯光调节失败（下位机未确认 OK: {resp}）"}

        except Exception as e:
            logger.error(f"[Agent] 台灯任务失败: {e}")
            return {"success": False, "message": f"灯光调节失败: {str(e)}"}

    async def _execute_face_task(self, session_id: str) -> dict:
        """③ 人脸识别追踪 — tasks.task_face（独占摄像头与串口）"""
        try:
            await self.send_to_client(session_id, {
                "type": "task_log",
                "message": "人脸追踪需独占串口与摄像头，准备切换...",
            })

            def worker():
                self._agent_script_worker_preamble()
                from arm import Arm
                from tasks import task_face

                arm = Arm(settings.robot_serial_port)
                try:
                    task_face(arm)
                finally:
                    arm.close()

            await robot_service.disconnect()
            try:
                await asyncio.to_thread(worker)
            finally:
                await robot_service.connect()

            await self.send_to_client(session_id, {
                "type": "task_log",
                "message": "人脸追踪已结束，串口已恢复。",
            })
            return {"success": True, "message": "人脸追踪流程已结束。"}
        except Exception as e:
            logger.error(f"[Agent] 人脸任务失败: {e}")
            try:
                await robot_service.connect()
            except Exception:
                pass
            return {"success": False, "message": f"人脸追踪失败: {str(e)}"}

    async def _execute_answer_task(
        self,
        session_id: str,
        params: dict,
        user_text: str,
    ) -> dict:
        """④ 题目解答 — tasks.task_answer（摄像头 + 多模态 + TTS）"""
        question = (params.get("question") or "").strip()
        if not question:
            question = user_text or "请解答图片中的题目"
        try:
            await self.send_to_client(session_id, {
                "type": "task_log",
                "message": "正在独占串口以运行拍题解答（agent/tasks.task_answer）...",
            })

            def worker():
                self._agent_script_worker_preamble()
                from arm import Arm
                from tasks import task_answer

                arm = Arm(settings.robot_serial_port)
                try:
                    task_answer(arm, question=question)
                finally:
                    arm.close()

            await robot_service.disconnect()
            try:
                await asyncio.to_thread(worker)
            finally:
                await robot_service.connect()

            return {"success": True, "message": "题目解答流程已执行完毕，请查看终端或收听语音播报。"}
        except Exception as e:
            logger.error(f"[Agent] 解答任务失败: {e}")
            try:
                await robot_service.connect()
            except Exception:
                pass
            return {"success": False, "message": f"题目分析失败: {str(e)}"}

    async def _execute_action_task(self, session_id: str, action_name: str) -> dict:
        """动作执行（agent.py 第 5 项）：与 tasks.task_action 同源 JSON，经 robot_service 下发 MOVE"""
        try:
            from app.database import async_session_maker

            extra_info = ""
            if action_name == "看天气":
                weather = await self._get_weather()
                extra_info = f"（天气提示：今天{weather}）"

            await self.send_to_client(session_id, {
                "type": "task_log",
                "message": f"正在回放动作「{action_name}」...{extra_info}",
            })

            if not await robot_service.is_online():
                return {"success": False, "message": "机械臂未连接"}

            async with async_session_maker() as db:
                result = await robot_service.play_action_sequence(db, action_name)

            ok = result.get("success", False)
            base_msg = result.get("message", "动作执行结束")
            return {
                "success": ok,
                "message": f"{base_msg}{extra_info}" if ok else base_msg,
                "frames": result.get("frames"),
            }
        except Exception as e:
            logger.error(f"[Agent] 动作任务失败: {e}")
            return {"success": False, "message": f"动作执行失败: {str(e)}"}

    async def _get_weather(self) -> str:
        """获取真实天气信息"""
        try:
            # 可以调用天气 API，这里用模拟数据
            import random
            weathers = ["晴朗", "多云", "阴天", "有小雨"]
            temps = random.randint(18, 30)
            return f"{random.choice(weathers)}，气温{ temps}度"
        except Exception:
            return "天气晴朗"


# 全局单例
agent_service = AgentService()
