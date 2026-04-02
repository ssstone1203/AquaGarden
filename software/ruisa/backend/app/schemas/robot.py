"""
机械臂相关 Pydantic 模型
"""
from datetime import datetime

from pydantic import BaseModel, Field


# ── 机械臂位置 ─────────────────────────────────────────────────────────────
class ArmPosition(BaseModel):
    """机械臂末端位置 (x, y, z, pitch)"""
    x: float = Field(description="X 坐标 (cm)")
    y: float = Field(description="Y 坐标 (cm)")
    z: float = Field(description="Z 坐标 (cm, 参考桌面高度)")
    pitch: float = Field(description="俯仰角 (°)")


# ── 标定 ──────────────────────────────────────────────────────────────────
class CalibrationResult(BaseModel):
    """标定结果"""
    name: str
    table_z: float | None = None
    block_height: float | None = None
    obs_x: float | None = None
    obs_y: float | None = None
    obs_z: float | None = None
    obs_pitch: float | None = None
    samples_count: int = 0
    created_at: datetime | None = None


class CalibrationSave(BaseModel):
    """保存标定数据"""
    name: str = Field("default", description="标定名称")
    table_z: float = Field(..., description="桌面高度 (cm)")
    block_height: float = Field(..., description="物块高度 (cm)")
    obs_x: float = Field(..., description="观测位 X (cm)")
    obs_y: float = Field(..., description="观测位 Y (cm)")
    obs_z: float = Field(..., description="观测位 Z (cm)")
    obs_pitch: float = Field(..., description="观测位俯仰角 (°)")
    affine_matrix: list[float] | None = Field(
        None,
        description="仿射变换矩阵（12个float，行优先4×3）",
    )
    teach_samples: list[dict] | None = Field(
        None,
        description="示教原始数据",
    )


class CalibrationResponse(BaseModel):
    """标定响应"""
    id: str
    name: str
    table_z: float | None = None
    block_height: float | None = None
    obs_x: float | None = None
    obs_y: float | None = None
    obs_z: float | None = None
    obs_pitch: float | None = None
    is_active: str
    created_at: datetime | None = None

    model_config = {"from_attributes": True}


# ── 颜色检测 ───────────────────────────────────────────────────────────────
class ColorDetectionRequest(BaseModel):
    """颜色检测请求"""
    camera_id: str = Field("robot", description="摄像头ID: robot | tank")
    colors: list[str] = Field(
        default=["red", "green", "blue"],
        description="待检测颜色列表",
    )


class ColorTargetResult(BaseModel):
    """单个颜色目标"""
    color: str
    center_x: int
    center_y: int
    confidence: float
    arm_x: float | None = Field(None, description="转换后的机械臂 X (cm)")
    arm_y: float | None = Field(None, description="转换后的机械臂 Y (cm)")
    arm_z: float | None = Field(None, description="转换后的机械臂 Z (cm)")
    arm_pitch: float | None = Field(None, description="转换后的俯仰角 (°)")


class ColorDetectionResponse(BaseModel):
    """颜色检测响应"""
    targets: list[ColorTargetResult]
    obs_position: ArmPosition | None = None


# ── 夹取任务 ───────────────────────────────────────────────────────────────
class ClampTaskRequest(BaseModel):
    """夹取分拣任务"""
    color: str = Field(..., description="目标颜色: red | green | blue")
    use_ultrasonic: bool = Field(True, description="是否使用超声波校正X轴")


class ClampStepResponse(BaseModel):
    """夹取步骤进度"""
    step: str
    status: str  # running | ok | error
    position: ArmPosition | None = None
    message: str | None = None


class ClampTaskResponse(BaseModel):
    """夹取任务完成响应"""
    success: bool
    color: str
    target_position: ArmPosition
    duration_seconds: float
    steps: list[str]


# ── 串口直接命令 ──────────────────────────────────────────────────────────────
class SerialCommandRequest(BaseModel):
    """串口直接命令"""
    command: str = Field(..., description="命令字: PING | RESET | UNLOAD | READ_POS | MOVE | GRIPPER_OPEN | GRIPPER_CLOSE | DIST")
    params: dict | None = Field(None, description="参数 dict")


class SerialCommandResponse(BaseModel):
    """串口命令响应"""
    command: str
    response: str
    ok: bool
    duration_ms: int
    timestamp: datetime


# ── 操作日志 ────────────────────────────────────────────────────────────────
class OperationLogEntry(BaseModel):
    """操作日志条目"""
    id: int
    action: str
    params: dict | None = None
    response: str | None = None
    result: str
    duration_ms: int | None = None
    created_at: datetime

    model_config = {"from_attributes": True}


# ── 实时状态 ────────────────────────────────────────────────────────────────
class ArmStatusResponse(BaseModel):
    """机械臂实时状态"""
    online: bool
    position: ArmPosition | None = None
    gripper_open: bool
    calibration_loaded: bool
    calibration_name: str | None = None
    last_command: str | None = None
    last_updated: datetime | None = None
