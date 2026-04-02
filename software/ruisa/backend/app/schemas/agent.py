"""
Agent 相关 Pydantic 模型
"""
from datetime import datetime
from enum import Enum
from typing import Optional
from pydantic import BaseModel, Field


class SessionState(str, Enum):
    """会话状态"""
    IDLE = "idle"
    WAITING_TASK = "waiting_task"
    EXECUTING = "executing"
    DIALOGUE = "dialogue"
    ENDED = "ended"


class TaskType(str, Enum):
    """任务类型"""
    CLAMP = "clamp"
    LED = "led"
    FACE = "face"
    ANSWER = "answer"
    ACTION = "action"


# ── 对话消息 ────────────────────────────────────────────────────────────────

class DialogueMessage(BaseModel):
    """对话消息"""
    role: str = Field(description="角色: user | assistant | system")
    content: str = Field(description="消息内容")
    timestamp: datetime = Field(default_factory=datetime.now)


class DialogueRequest(BaseModel):
    """对话请求"""
    text: str = Field(..., description="用户输入文本")
    image_data: Optional[str] = Field(None, description="可选的 base64 图片数据")


class DialogueResponse(BaseModel):
    """对话响应"""
    response: str = Field(..., description="AI 响应文本")
    task_triggered: bool = Field(..., description="是否触发了任务")
    task_name: Optional[str] = Field(None, description="任务名")
    task_params: dict = Field(default_factory=dict, description="任务参数")
    session_state: SessionState = Field(..., description="当前会话状态")
    should_end: bool = Field(..., description="是否应结束会话")
    timestamp: datetime = Field(default_factory=datetime.now)


# ── 会话状态 ────────────────────────────────────────────────────────────────

class SessionStatus(BaseModel):
    """会话状态"""
    session_id: str = Field(..., description="会话ID")
    state: SessionState = Field(..., description="会话状态")
    current_task: Optional[str] = Field(None, description="当前任务")
    message_count: int = Field(0, description="消息数量")
    created_at: datetime = Field(..., description="创建时间")


class SessionList(BaseModel):
    """会话列表"""
    sessions: list[SessionStatus] = Field(default_factory=list)
    total: int = Field(0, description="总数")


# ── 任务相关 ────────────────────────────────────────────────────────────────

class TaskStart(BaseModel):
    """任务开始"""
    task: TaskType = Field(..., description="任务类型")
    params: dict = Field(default_factory=dict, description="任务参数")


class TaskLog(BaseModel):
    """任务日志"""
    task: str = Field(..., description="任务名")
    message: str = Field(..., description="日志消息")
    level: str = Field("INFO", description="日志级别")


class TaskProgress(BaseModel):
    """任务进度"""
    task: str = Field(..., description="任务名")
    step: str = Field(..., description="当前步骤")
    progress: float = Field(0.0, description="进度百分比 0-100")
    message: str = Field(..., description="进度消息")


class TaskComplete(BaseModel):
    """任务完成"""
    task: str = Field(..., description="任务名")
    success: bool = Field(..., description="是否成功")
    message: str = Field(..., description="完成消息")
    duration_seconds: float = Field(0.0, description="耗时秒数")


class TaskError(BaseModel):
    """任务错误"""
    task: str = Field(..., description="任务名")
    error: str = Field(..., description="错误信息")


# ── WebSocket 消息 ─────────────────────────────────────────────────────────

class WSMessageSend(BaseModel):
    """WebSocket 发送消息"""
    type: str = Field(..., description="消息类型: message | ping | end_session")
    text: Optional[str] = Field(None, description="文本内容")
    image_data: Optional[str] = Field(None, description="base64 图片")


class WSMessageReceive(BaseModel):
    """WebSocket 接收消息"""
    type: str = Field(..., description="消息类型")
    response: Optional[str] = Field(None, description="AI 响应")
    task_triggered: Optional[bool] = Field(None, description="是否触发任务")
    task_name: Optional[str] = Field(None, description="任务名")
    task_params: Optional[dict] = Field(None, description="任务参数")
    session_state: Optional[SessionState] = Field(None, description="会话状态")
    should_end: Optional[bool] = Field(None, description="是否结束")
    message: Optional[str] = Field(None, description="日志/进度消息")
    success: Optional[bool] = Field(None, description="任务成功标志")
    error: Optional[str] = Field(None, description="错误信息")
    timestamp: datetime = Field(default_factory=datetime.now)
