"""
机械臂相关模型：标定数据 + 操作日志
"""
import json
import uuid
from datetime import datetime, timezone
from typing import List, Optional

from sqlalchemy import DateTime, Float, Integer, String, Text, JSON
from sqlalchemy.orm import Mapped, mapped_column

from app.database import Base


class RobotCalibration(Base):
    """机械臂标定数据表"""
    __tablename__ = "robot_calibrations"

    id: Mapped[str] = mapped_column(
        String(36),
        primary_key=True,
        default=lambda: str(uuid.uuid4()),
    )
    name: Mapped[str] = mapped_column(
        String(50),
        nullable=False,
        unique=True,
        index=True,
    )
    # 仿射变换矩阵（4×3，行优先存储，共12个float）
    affine_matrix: Mapped[Optional[List[float]]] = mapped_column(
        JSON,
        nullable=True,
    )
    # 观测位姿（示教采集时的机械臂固定坐标）
    obs_x: Mapped[Optional[float]] = mapped_column(Float, nullable=True)
    obs_y: Mapped[Optional[float]] = mapped_column(Float, nullable=True)
    obs_z: Mapped[Optional[float]] = mapped_column(Float, nullable=True)
    obs_pitch: Mapped[Optional[float]] = mapped_column(Float, nullable=True)
    # 高度标定
    table_z: Mapped[Optional[float]] = mapped_column(
        Float,
        nullable=True,
        comment="桌面高度（cm，相对机械臂坐标系）",
    )
    block_height: Mapped[Optional[float]] = mapped_column(
        Float,
        nullable=True,
        comment="物块高度（cm）",
    )
    # 示教采集的原始数据（每组：像素坐标 + 机械臂坐标）
    teach_samples: Mapped[Optional[dict]] = mapped_column(
        JSON,
        nullable=True,
    )
    is_active: Mapped[bool] = mapped_column(
        String(10),
        default="true",
        nullable=False,
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        onupdate=lambda: datetime.now(timezone.utc),
        nullable=False,
    )

    def __repr__(self) -> str:
        return f"<RobotCalibration {self.name}>"


class RobotOperationLog(Base):
    """机械臂操作日志表"""
    __tablename__ = "robot_operation_logs"
    __table_args__ = {"sqlite_autoincrement": True}

    id: Mapped[int] = mapped_column(
        primary_key=True,
        autoincrement=True,
    )
    # 操作类型：move | gripper_open | gripper_close | read_pos | ping | dist | calibrate | ...
    action: Mapped[str] = mapped_column(String(50), nullable=False, index=True)
    # 操作参数
    params: Mapped[Optional[dict]] = mapped_column(JSON, nullable=True)
    # 机械臂响应
    response: Mapped[Optional[str]] = mapped_column(Text, nullable=True)
    # 执行结果：ok | timeout | error
    result: Mapped[str] = mapped_column(String(20), nullable=False, default="ok")
    # 耗时（秒）
    duration_ms: Mapped[Optional[int]] = mapped_column(Integer, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
