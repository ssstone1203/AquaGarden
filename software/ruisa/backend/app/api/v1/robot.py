"""
机械臂控制路由

所有接口均基于 RA6M5 串口协议实现，完整对应以下命令：
  PING | RESET | UNLOAD | READ_POS | MOVE | GRIPPER_OPEN | GRIPPER_CLOSE | DIST

API 路径前缀：/api/v1/arm
"""
from datetime import datetime

from fastapi import APIRouter, Depends, File, HTTPException, Query, UploadFile, status
from fastapi.responses import StreamingResponse

from app.deps import CurrentUser, DbSession
from app.schemas.common import PaginatedData
from app.schemas.robot import (
    ArmStatusResponse,
    CalibrationResponse,
    CalibrationSave,
    ClampTaskRequest,
    ClampTaskResponse,
    ColorDetectionResponse,
    OperationLogEntry,
    SerialCommandRequest,
    SerialCommandResponse,
)
from app.services.robot_service import robot_service

router = APIRouter()


# ── 串口连接 ───────────────────────────────────────────────────────────────
@router.post("/connect")
async def connect_arm(current_user: CurrentUser):
    """建立串口连接（连接 RA6M5 机械臂）"""
    result = await robot_service.connect()
    if not result["connected"]:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="串口连接失败，请检查设备是否连接",
        )
    return result


@router.post("/disconnect")
async def disconnect_arm(current_user: CurrentUser):
    """断开串口连接"""
    await robot_service.disconnect()
    return {"message": "已断开串口连接"}


# ── 状态 ─────────────────────────────────────────────────────────────────
@router.get("/status", response_model=ArmStatusResponse)
async def get_arm_status(current_user: CurrentUser):
    """获取机械臂实时状态（在线状态、位置、夹爪、标定）"""
    return await robot_service.get_status()


# ── 串口直接命令 ──────────────────────────────────────────────────────────
@router.post("/command", response_model=SerialCommandResponse)
async def send_command(
    data: SerialCommandRequest,
    db: DbSession,
    current_user: CurrentUser,
):
    """
    发送串口命令到机械臂。

    支持命令：
    | 命令 | 说明 | 参数 |
    |------|------|------|
    | PING | 心跳检测 | 无 |
    | RESET | 归零复位 | 无 |
    | UNLOAD | 舵机卸力 | 无 |
    | READ_POS | 读取当前位置 | 无 |
    | MOVE | 移动到坐标 | x, y, z, pitch |
    | GRIPPER_OPEN | 张开夹爪 | 无 |
    | GRIPPER_CLOSE | 闭合夹爪 | 无 |
    | DIST | 超声波测距 | 无 |
    """
    return await robot_service.send_command(db, data.command, data.params)


# ── 标定 ────────────────────────────────────────────────────────────────
@router.get("/calibrations")
async def list_calibrations(
    db: DbSession,
    current_user: CurrentUser,
):
    """获取所有标定数据"""
    calibs = await robot_service.get_calibrations(db)
    return [
        CalibrationResponse(
            id=c.id,
            name=c.name,
            table_z=c.table_z,
            block_height=c.block_height,
            obs_x=c.obs_x,
            obs_y=c.obs_y,
            obs_z=c.obs_z,
            obs_pitch=c.obs_pitch,
            is_active=c.is_active,
            created_at=c.created_at,
        )
        for c in calibs
    ]


@router.post("/calibrations")
async def save_calibration(
    data: CalibrationSave,
    db: DbSession,
    current_user: CurrentUser,
):
    """
    保存标定数据。
    对应 software/target/clamp/collect_teach.py 和 teach.py 的标定结果。
    """
    calib = await robot_service.save_calibration(db, data.model_dump())
    # 自动加载
    await robot_service.load_calibration(db, data.name)
    return CalibrationResponse(
        id=calib.id,
        name=calib.name,
        table_z=calib.table_z,
        block_height=calib.block_height,
        obs_x=calib.obs_x,
        obs_y=calib.obs_y,
        obs_z=calib.obs_z,
        obs_pitch=calib.obs_pitch,
        is_active=calib.is_active,
        created_at=calib.created_at,
    )


@router.post("/calibrations/{name}/load")
async def load_calibration(
    name: str,
    db: DbSession,
    current_user: CurrentUser,
):
    """加载指定标定数据到内存（使颜色检测可使用仿射矩阵）"""
    ok = await robot_service.load_calibration(db, name)
    if not ok:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"标定 '{name}' 不存在",
        )
    return {"message": f"标定 '{name}' 已加载", "name": name}


# ── 颜色检测 ─────────────────────────────────────────────────────────────
@router.post("/detect", response_model=ColorDetectionResponse)
async def detect_colors(
    current_user: CurrentUser,
    db: DbSession,
    file: UploadFile = File(...),
    colors: str = Query(
        default="red,green,blue",
        description="待检测颜色，逗号分隔，如 red,green,blue",
    ),
):
    """
    上传摄像头图像帧，检测指定颜色并转换为机械臂坐标。
    对应 clamp.py 中的 detect_block() + pixel_to_arm()。

    颜色支持：red | green | blue
    """
    import numpy as np
    import cv2

    contents = await file.read()
    nparr = np.frombuffer(contents, np.uint8)
    frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

    if frame is None:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="无法解码图像，请上传有效的 JPEG/PNG 图像",
        )

    colors_list = [c.strip() for c in colors.split(",")] if colors else None

    return await robot_service.detect_colors(db, frame, colors_list)


# ── 夹取分拣任务 ─────────────────────────────────────────────────────────
@router.post("/clamp", response_model=ClampTaskResponse)
async def run_clamp_task(
    data: ClampTaskRequest,
    db: DbSession,
    current_user: CurrentUser,
):
    """
    执行完整的彩色物块夹取分拣任务。

    完整流程（共12步）：
    1. 移到观测位
    2. 颜色检测定位目标（前置调用 /detect）
    3. 移到目标上方（安全高度）
    4. 超声波 X 轴校正（可选）
    5. 张开夹爪
    6. 下降到夹取高度
    7. 闭合夹爪
    8. 抬起到安全高度
    9. 水平移动到分拣区正上方
    10. 下降到放置高度
    11. 松开夹爪
    12. 归零

    分拣区域坐标（clamp.py 预设）：
    | 颜色 | X | Y | Z | Pitch |
    |------|------|------|------|--------|
    | 红色 | 8.31 | 21.78 | -6.54 | -74.2° |
    | 绿色 | 0.84 | 22.21 | -7.01 | -78.0° |
    | 蓝色 | -4.36 | 22.34 | -7.24 | -72.2° |
    """
    return await robot_service.run_clamp_task(db, data.color, data.use_ultrasonic)


# ── 操作日志 ─────────────────────────────────────────────────────────────
@router.get("/logs")
async def get_operation_logs(
    current_user: CurrentUser,
    db: DbSession,
    page: int = Query(1, ge=1),
    page_size: int = Query(50, ge=1, le=200),
):
    """获取机械臂操作日志（串口命令记录）"""
    logs, total = await robot_service.get_logs(db, page, page_size)
    total_pages = (total + page_size - 1) // page_size

    return PaginatedData(
        total=total,
        page=page,
        page_size=page_size,
        total_pages=total_pages,
        data=[OperationLogEntry.model_validate(log) for log in logs],
    )
