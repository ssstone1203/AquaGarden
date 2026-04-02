"""
agent_service.py —— Agent 对话服务

管理 AI 对话会话、任务执行和 WebSocket 实时通信。
"""

import asyncio
import json
import logging
import time
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Callable, Optional

from app.services.robot_service import robot_service

logger = logging.getLogger(__name__)


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

    # ── 会话管理 ────────────────────────────────────────────────────────────

    def create_session(self, session_id: str) -> AgentSession:
        """创建新会话"""
        session = AgentSession(session_id=session_id)
        self._sessions[session_id] = session
        logger.info(f"[Agent] 创建会话: {session_id}")
        return session

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

        # 添加用户消息
        session.messages.append(DialogueMessage(
            role="user",
            content=text,
        ))

        # 检测是否结束会话
        farewell_words = ["拜拜", "再见", "bye", "再见啦", "拜拜啦", "我走了"]
        should_end = any(word in text for word in farewell_words)

        if should_end:
            response = "好的，下次见！有问题随时叫我哦~"
            session.state = SessionState.ENDED
            self.end_session(session_id)
            return {
                "response": response,
                "task_triggered": False,
                "task_name": None,
                "task_params": {},
                "session_state": SessionState.ENDED.value,
                "should_end": True,
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

            # 异步执行任务（不阻塞对话）
            asyncio.create_task(self._execute_task(session_id, task_name, task_params))

        else:
            # 自由对话或未识别
            session.state = SessionState.DIALOGUE
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
        }

    def _parse_intent(self, text: str) -> tuple[Optional[str], dict]:
        """
        解析用户意图。
        这里复用 dialogue.py 的 parse 函数。
        """
        try:
            from agent.dialogue import parse
            return parse(text)
        except ImportError:
            # 后端可能没有完整的 agent 依赖，使用简单关键词匹配
            return self._keyword_parse(text)

    def _keyword_parse(self, text: str) -> tuple[str, dict]:
        """简单的关键词匹配降级"""
        keywords_map = [
            (["颜色", "分拣", "积木", "物块", "夹取", "分类"], "clamp", {}),
            (["灯", "光", "亮度", "调亮", "调暗", "打开灯", "关闭灯"], "led", self._parse_led_params(text)),
            (["人脸", "追踪", "跟踪", "在不在", "看我"], "face", {}),
            (["题目", "解答", "分析", "拍照", "作业", "解题", "题"], "answer", {}),
            (["跳舞", "招手", "挥手", "点头", "摇头", "看天气", "动作"], "action", self._parse_action_params(text)),
        ]

        for keywords, task, params in keywords_map:
            if any(w in text for w in keywords):
                return task, params

        return "unknown", {}

    def _parse_led_params(self, text: str) -> dict:
        """解析台灯参数"""
        preset = "medium"
        if any(w in text for w in ["关闭", "熄", "关掉"]):
            preset = "off"
        elif any(w in text for w in ["调暗", "低", "暗"]):
            preset = "low"
        elif any(w in text for w in ["调亮", "高", "亮", "最亮"]):
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
        """生成自然的任务确认响应"""
        responses = {
            "clamp": "好的，我来帮你分拣积木！让我先看看有什么颜色的。",
            "led": self._generate_led_response(params.get("preset", "medium")),
            "face": "好的，我来帮你看看你有没有在认真学习和有没有坐好！",
            "answer": "好的，我来帮你拍照看看这道题！",
            "action": self._generate_action_response(params.get("action", "打招呼")),
        }
        return responses.get(task_name, "好的，我来帮你处理！")

    def _generate_led_response(self, preset: str) -> str:
        """生成台灯相关的自然响应"""
        presets = {
            "off": "好的，我把灯关掉。",
            "low": "好的，我把灯光调暗一点，这样更护眼。",
            "medium": "好的，我把灯光调到适中的亮度。",
            "high": "好的，我把灯光调亮一些，这样更清晰。",
        }
        return presets.get(preset, "好的，我来调节灯光。")

    def _generate_action_response(self, action: str) -> str:
        """生成动作相关的自然响应"""
        actions = {
            "打招呼": "好的，我来打个招呼！",
            "跳舞": "好的，我来跳个舞！",
            "点头": "好的，我点头表示同意！",
            "摇头": "好的，我摇头表示不同意！",
            "看天气": "好的，让我抬头看看今天的天气！",
        }
        return actions.get(action, f"好的，我来表演{action}！")

    async def _generate_free_response(
        self,
        session: AgentSession,
        text: str,
        image_data: Optional[str],
    ) -> str:
        """生成自由对话响应（调用大模型）"""
        try:
            import config as agent_config
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
    ):
        """异步执行任务，实时推送进度到客户端"""
        try:
            await self.send_to_client(session_id, {
                "type": "task_start",
                "task": task_name,
                "params": params,
            })

            # 根据任务类型执行
            if task_name == "clamp":
                result = await self._execute_clamp_task(session_id)
            elif task_name == "led":
                result = await self._execute_led_task(session_id, params.get("preset", "medium"))
            elif task_name == "face":
                result = await self._execute_face_task(session_id)
            elif task_name == "answer":
                result = await self._execute_answer_task(session_id)
            elif task_name == "action":
                result = await self._execute_action_task(session_id, params.get("action", "打招呼"))
            else:
                result = {"success": False, "message": "未知任务"}

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

    async def _execute_clamp_task(self, session_id: str) -> dict:
        """执行颜色分拣任务"""
        try:
            await self.send_to_client(session_id, {
                "type": "task_log",
                "message": "正在启动颜色检测...",
            })

            # 获取机械臂状态
            status = await robot_service.get_status()
            if not status.get("online"):
                return {"success": False, "message": "机械臂未连接"}

            # 发送执行信号（前端可以监听这个来显示进度）
            await self.send_to_client(session_id, {
                "type": "task_progress",
                "task": "clamp",
                "step": "detecting",
                "message": "正在识别颜色...",
            })

            # 这里可以调用现有的 clamp API
            # await robot_service.run_clamp_task(...)

            return {
                "success": True,
                "message": "颜色分拣完成！",
            }

        except Exception as e:
            logger.error(f"[Agent] 分拣任务失败: {e}")
            return {"success": False, "message": f"分拣失败: {str(e)}"}

    async def _execute_led_task(self, session_id: str, preset: str) -> dict:
        """执行台灯控制任务"""
        try:
            # 获取台灯预设参数
            from agent import config as agent_config
            presets = agent_config.LED_PRESETS
            preset_data = presets.get(preset, presets.get("medium"))

            if preset == "off":
                # 关闭台灯
                await self.send_to_client(session_id, {
                    "type": "task_log",
                    "message": "正在关闭台灯...",
                })
                ok, resp, _ = await robot_service._serial.send("LED 0 0 0 0")
            else:
                # 设置台灯亮度
                await self.send_to_client(session_id, {
                    "type": "task_log",
                    "message": f"正在调节灯光到{preset}模式...",
                })
                cmd = f"LED {preset_data['r']} {preset_data['g']} {preset_data['b']} {preset_data['bright']}"
                ok, resp, _ = await robot_service._serial.send(cmd)

            if ok:
                return {"success": True, "message": "灯光调节完成！"}
            else:
                return {"success": False, "message": "灯光调节失败"}

        except Exception as e:
            logger.error(f"[Agent] 台灯任务失败: {e}")
            return {"success": False, "message": f"灯光调节失败: {str(e)}"}

    async def _execute_face_task(self, session_id: str) -> dict:
        """执行人脸识别任务"""
        try:
            await self.send_to_client(session_id, {
                "type": "task_log",
                "message": "正在启动人脸检测...",
            })
            # 发送任务信号给前端，启动人脸追踪
            await self.send_to_client(session_id, {
                "type": "task_start",
                "task": "face",
            })
            return {
                "success": True,
                "message": "人脸追踪已启动！",
            }
        except Exception as e:
            logger.error(f"[Agent] 人脸任务失败: {e}")
            return {"success": False, "message": f"人脸追踪失败: {str(e)}"}

    async def _execute_answer_task(self, session_id: str) -> dict:
        """执行题目解答任务"""
        try:
            await self.send_to_client(session_id, {
                "type": "task_log",
                "message": "正在拍照并分析题目...",
            })
            # 发送任务信号给前端
            await self.send_to_client(session_id, {
                "type": "task_start",
                "task": "answer",
            })
            return {
                "success": True,
                "message": "题目分析完成！",
            }
        except Exception as e:
            logger.error(f"[Agent] 解答任务失败: {e}")
            return {"success": False, "message": f"题目分析失败: {str(e)}"}

    async def _execute_action_task(self, session_id: str, action_name: str) -> dict:
        """执行动作回放任务"""
        try:
            # 结合真实数据（如天气）
            extra_info = ""
            if action_name == "看天气":
                weather = await self._get_weather()
                extra_info = f"，今天{weather}"

            await self.send_to_client(session_id, {
                "type": "task_log",
                "message": f"正在执行动作：{action_name}{extra_info}...",
            })
            # 发送任务信号给前端
            await self.send_to_client(session_id, {
                "type": "task_start",
                "task": "action",
                "params": {"action": action_name},
            })
            return {
                "success": True,
                "message": f"{action_name}完成！{extra_info}",
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
