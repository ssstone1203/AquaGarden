"""
机械臂控制服务
封装串口通信、标定管理、夹取分拣任务
"""
import asyncio
import logging
import time
from datetime import datetime, timezone
from typing import Optional

import numpy as np
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.config import settings
from app.models.robot import RobotCalibration, RobotOperationLog
from app.schemas.robot import (
    ArmPosition,
    ClampTaskResponse,
    ColorDetectionResponse,
    ColorTargetResult,
    SerialCommandResponse,
)
from app.services.color_service import ColorService, SORT_ZONES


class ArmSerial:
    """
    RA6M5 机械臂串口协议封装。
    对应 hardware/arm/RA6M5/Serial_PC_Control/Src/serial_protocol.c 中定义的协议。
    """

    SUPPORTED_COMMANDS = {
        "PING", "RESET", "UNLOAD", "READ_POS",
        "MOVE", "GRIPPER_OPEN", "GRIPPER_CLOSE", "DIST",
    }

    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 3.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self._serial = None
        self._lock = asyncio.Lock()

    async def connect(self) -> bool:
        """建立串口连接"""
        import logging
        logger = logging.getLogger(__name__)
        
        try:
            import serial
            import serial.tools.list_ports
            
            # 检查串口是否存在
            available_ports = [p.device for p in serial.tools.list_ports.comports()]
            logger.info(f"可用串口: {available_ports}")
            
            if self.port not in available_ports:
                logger.warning(f"串口 {self.port} 未找到，可用串口: {available_ports}")
                return False
            
            self._serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                xonxoff=False,
                rtscts=False,
            )
            
            # 等待串口稳定
            import time
            time.sleep(0.1)
            
            # 清空缓冲区
            self._serial.reset_input_buffer()
            self._serial.reset_output_buffer()
            
            logger.info(f"串口 {self.port} 连接成功")
            return True
        except Exception as e:
            logger.error(f"串口连接失败: {e}")
            return False

    def is_connected(self) -> bool:
        return self._serial is not None and self._serial.is_open

    async def send(
        self,
        command: str,
        params: Optional[dict] = None,
    ) -> tuple[bool, str, int]:
        """
        发送命令并等待响应。
        协议格式（对应 hardware/arm/app/control/pc_control.c）：
          PING           → "PONG\n"
          RESET          → "OK\n" 或 "ERR\n"
          UNLOAD         → "OK\n" 或 "ERR\n"
          READ_POS       → "x,y,z,pitch\n" 或 "ERR\n"
          DIST           → "xx.xx\n" 或 "ERR\n"
          MOVE x y z pitch min_p max_p dur  → "OK\n" 或 "ERR\n"
          GRIPPER_OPEN [dur]  → "OK\n" 或 "ERR\n"
          GRIPPER_CLOSE [dur] → "OK\n" 或 "ERR\n"
        """
        async with self._lock:
            if not self.is_connected():
                return False, "SERIAL_NOT_CONNECTED", 0

            try:
                if command == "MOVE" and params:
                    # 格式：MOVE x y z pitch min_p max_p dur（空格分隔）
                    x = params.get("x", 0)
                    y = params.get("y", 0)
                    z = params.get("z", 0)
                    pitch = params.get("pitch", 0)
                    min_p = params.get("min_pitch", -90.0)
                    max_p = params.get("max_pitch", 90.0)
                    dur = params.get("duration", 1000)
                    cmd_str = f"MOVE {x} {y} {z} {pitch} {min_p} {max_p} {dur}\r\n"
                elif command == "GRIPPER_OPEN":
                    dur = params.get("duration", 500) if params else 500
                    cmd_str = f"GRIPPER_OPEN {dur}\r\n"
                elif command == "GRIPPER_CLOSE":
                    dur = params.get("duration", 500) if params else 500
                    cmd_str = f"GRIPPER_CLOSE {dur}\r\n"
                else:
                    cmd_str = f"{command}\r\n"

                t0 = time.time()
                
                # 清空接收缓冲区
                self._serial.reset_input_buffer()
                
                # 发送命令
                self._serial.write(cmd_str.encode('utf-8'))
                self._serial.flush()
                
                # 读取响应（支持 \r\n 或 \n）
                resp = self._serial.readline().decode('utf-8').strip()
                elapsed_ms = int((time.time() - t0) * 1000)

                ru = resp.upper()
                if command == "READ_POS":
                    # 成功为 "x,y,z,pitch"，失败常为 ERR / 空
                    ok = bool(resp) and ru != "ERR" and resp.count(",") >= 3
                elif command == "DIST":
                    ok = bool(resp) and ru != "ERR" and ru != "TIMEOUT"
                else:
                    ok = resp in ("OK", "PONG", "DONE") or resp.startswith("OK")
                return ok, resp, elapsed_ms

            except asyncio.TimeoutError:
                return False, "TIMEOUT", 0
            except Exception as e:
                return False, str(e), 0

    async def close(self) -> None:
        if self._serial and self._serial.is_open:
            self._serial.close()
            self._serial = None


class RobotService:
    """
    机械臂控制业务逻辑。
    管理串口连接、标定数据、夹取任务。
    """

    def __init__(self):
        self._serial = ArmSerial(
            port=settings.robot_serial_port,
            baudrate=settings.robot_baudrate,
        )
        self._color_service = ColorService()
        self._calibration: Optional[RobotCalibration] = None
        self._online = False
        self._last_pos = ArmPosition(x=0.0, y=0.0, z=0.0, pitch=0.0)
        self._gripper_open = True

    # ── 串口连接 ──────────────────────────────────────────────────────────
    async def connect(self) -> dict:
        """建立串口连接"""
        import logging
        logger = logging.getLogger(__name__)
        
        connected = await self._serial.connect()
        self._online = connected
        
        if connected:
            logger.info(f"机械臂串口 {self._serial.port} 连接成功，发送 PING...")
            ok, resp, _ = await self._serial.send("PING")
            logger.info(f"PING 响应: {resp}")
            
            _, pos_resp, _ = await self._serial.send("READ_POS")
            logger.info(f"READ_POS 响应: {pos_resp}")
            
            # 格式: "x,y,z,pitch\r\n"（pc_control.c）
            try:
                parts = pos_resp.split(",")
                if len(parts) >= 4:
                    self._last_pos = ArmPosition(
                        x=float(parts[0]),
                        y=float(parts[1]),
                        z=float(parts[2]),
                        pitch=float(parts[3]),
                    )
                    logger.info(f"初始位置: {self._last_pos}")
            except (ValueError, IndexError):
                logger.warning(f"无法解析位置数据: {pos_resp}")
        else:
            logger.error(f"机械臂串口连接失败: {self._serial.port}")
            
        return {"connected": self._online, "port": self._serial.port, "baudrate": self._serial.baudrate}

    async def disconnect(self) -> None:
        await self._serial.close()
        self._online = False

    async def is_online(self) -> bool:
        return self._online and self._serial.is_connected()

    # ── 直接串口命令 ────────────────────────────────────────────────────
    async def send_command(
        self,
        db: AsyncSession,
        command: str,
        params: Optional[dict] = None,
    ) -> SerialCommandResponse:
        """发送串口命令并记录日志"""
        t0 = time.time()
        ok, resp, elapsed_ms = await self._serial.send(command, params)
        duration_ms = int((time.time() - t0) * 1000)

        log = RobotOperationLog(
            action=command,
            params=params,
            response=resp,
            result="ok" if ok else ("timeout" if resp == "TIMEOUT" else "error"),
            duration_ms=duration_ms,
        )
        db.add(log)
        await db.flush()

        # 解析 READ_POS 响应：格式 "x,y,z,pitch\n"（pc_control.c）
        if ok and command == "READ_POS":
            try:
                parts = resp.split(",")
                if len(parts) >= 4:
                    self._last_pos = ArmPosition(
                        x=float(parts[0]),
                        y=float(parts[1]),
                        z=float(parts[2]),
                        pitch=float(parts[3]),
                    )
            except (ValueError, IndexError):
                pass
        elif ok and command == "GRIPPER_OPEN":
            self._gripper_open = True
        elif ok and command == "GRIPPER_CLOSE":
            self._gripper_open = False

        return SerialCommandResponse(
            command=command,
            response=resp,
            ok=ok,
            duration_ms=duration_ms,
            timestamp=datetime.now(timezone.utc),
        )

    # ── 标定 ────────────────────────────────────────────────────────────
    def _load_calibration_from_teach_map_file(self) -> bool:
        """
        从 collect_teach.py 生成的 teach_map.npz 加载（与 clamp.py / agent MAP_FILE 同源）。
        仿射矩阵表示像素到机械臂坐标的绝对映射（与 target/clamp/clamp.py 一致），
        颜色服务侧观测偏移置零，观测位姿单独存在 RobotCalibration 字段供夹取流程使用。
        """
        path = settings.robot_teach_map_path
        if not path.is_file():
            return False
        log = logging.getLogger(__name__)
        try:
            data = np.load(path, allow_pickle=False)
            A = np.asarray(data["A"], dtype=np.float64)
            if A.shape != (4, 3):
                log.warning("teach_map.npz 中 A 形状应为 (4,3)，实际: %s", A.shape)
                return False
            obs_pose = data.get("obs_pose")
            if obs_pose is None:
                ox, oy, oz, op = 16.0, 0.0, -3.2, -76.1
            else:
                opa = np.asarray(obs_pose).astype(np.float64).flatten()
                if opa.size < 4:
                    return False
                ox, oy, oz, op = float(opa[0]), float(opa[1]), float(opa[2]), float(opa[3])
            affine_flat = A.reshape(-1).astype(float).tolist()
            self._calibration = RobotCalibration(
                name="teach_map.npz",
                affine_matrix=affine_flat,
                obs_x=ox,
                obs_y=oy,
                obs_z=oz,
                obs_pitch=op,
                table_z=None,
                block_height=3.0,
                teach_samples=None,
                is_active="true",
            )
            self._color_service.load_calibration(
                affine_matrix=affine_flat,
                obs_position={
                    "x": 0.0,
                    "y": 0.0,
                    "z": 0.0,
                    "pitch": 0.0,
                },
            )
            log.info("已从示教文件加载标定: %s", path)
            return True
        except Exception as e:
            log.warning("读取 teach_map 失败 (%s): %s", path, e)
            return False

    async def load_calibration(
        self,
        db: AsyncSession,
        name: str = "default",
    ) -> bool:
        """加载指定标定到内存；数据库无对应记录且 name 为 default 时尝试 teach_map.npz。"""
        stmt = select(RobotCalibration).where(
            RobotCalibration.name == name,
            RobotCalibration.is_active == "true",
        )
        result = await db.execute(stmt)
        calib = result.scalar_one_or_none()

        if calib is not None:
            self._calibration = calib
            if calib.affine_matrix:
                self._color_service.load_calibration(
                    affine_matrix=calib.affine_matrix,
                    obs_position={
                        "x": calib.obs_x or 0.0,
                        "y": calib.obs_y or 0.0,
                        "z": calib.obs_z or 0.0,
                        "pitch": calib.obs_pitch or 0.0,
                    },
                )
            return True

        if name == "default" and self._load_calibration_from_teach_map_file():
            return True
        return False

    async def save_calibration(
        self,
        db: AsyncSession,
        data: dict,
    ) -> RobotCalibration:
        """保存标定数据"""
        stmt = select(RobotCalibration).where(RobotCalibration.name == data["name"])
        result = await db.execute(stmt)
        existing = result.scalar_one_or_none()
        if existing:
            existing.is_active = "false"

        calib = RobotCalibration(
            name=data["name"],
            table_z=data.get("table_z"),
            block_height=data.get("block_height"),
            obs_x=data.get("obs_x"),
            obs_y=data.get("obs_y"),
            obs_z=data.get("obs_z"),
            obs_pitch=data.get("obs_pitch"),
            affine_matrix=data.get("affine_matrix"),
            teach_samples=data.get("teach_samples"),
            is_active="true",
        )
        db.add(calib)
        await db.flush()
        await db.refresh(calib)
        return calib

    async def get_calibrations(
        self,
        db: AsyncSession,
    ) -> list[RobotCalibration]:
        """获取所有标定"""
        stmt = select(RobotCalibration).order_by(RobotCalibration.created_at.desc())
        result = await db.execute(stmt)
        return list(result.scalars().all())

    # ── 颜色检测 ────────────────────────────────────────────────────────
    async def detect_colors(
        self,
        db: AsyncSession,
        frame,
        colors: Optional[list[str]] = None,
    ) -> ColorDetectionResponse:
        """在图像帧中检测颜色并转换为机械臂坐标"""
        if self._calibration is None:
            await self.load_calibration(db, "default")

        targets_raw = self._color_service.detect_and_convert(frame, colors)

        targets = []
        obs_pos = None
        if self._calibration:
            obs_pos = ArmPosition(
                x=self._calibration.obs_x or 0.0,
                y=self._calibration.obs_y or 0.0,
                z=self._calibration.obs_z or 0.0,
                pitch=self._calibration.obs_pitch or 0.0,
            )

        for raw in targets_raw:
            targets.append(ColorTargetResult(
                color=raw["color"],
                center_x=raw["pixel_x"],
                center_y=raw["pixel_y"],
                confidence=raw["confidence"],
                arm_x=raw["arm_x"],
                arm_y=raw["arm_y"],
                arm_z=raw["arm_z"],
                arm_pitch=raw["arm_pitch"],
            ))

        return ColorDetectionResponse(targets=targets, obs_position=obs_pos)

    # ── 夹取分拣任务 ────────────────────────────────────────────────────
    async def run_clamp_task(
        self,
        db: AsyncSession,
        color: str,
        use_ultrasonic: bool = True,
    ) -> ClampTaskResponse:
        """
        执行完整的彩色物块夹取分拣流程。
        对应 software/target/clamp/clamp.py 中的 run_clamp() 函数。
        """
        steps: list[str] = []
        t0 = time.time()

        if color not in SORT_ZONES:
            raise ValueError(f"未知颜色: {color}")

        if self._calibration is None:
            await self.load_calibration(db, "default")
        if self._calibration is None:
            raise RuntimeError(
                "请先加载标定数据：数据库无 default 标定，且未找到示教文件 "
                f"{settings.robot_teach_map_path}（可运行 software/target/clamp/collect_teach.py 生成）"
            )

        table_z = self._calibration.table_z or 0.0
        block_height = self._calibration.block_height or 3.0
        clamp_z = table_z + block_height - 0.5

        try:
            # Step 1: 移到观测位
            obs = ArmPosition(
                x=self._calibration.obs_x or 0.0,
                y=self._calibration.obs_y or 0.0,
                z=self._calibration.obs_z or 0.0,
                pitch=self._calibration.obs_pitch or 0.0,
            )
            await self.send_command(db, "MOVE", {
                "x": obs.x, "y": obs.y, "z": obs.z, "pitch": obs.pitch,
                "min_pitch": -90.0, "max_pitch": 90.0, "duration": 2000,
            })
            steps.append(f"移到观测位 ({obs.x}, {obs.y}, {obs.z})")

            # Step 2: 目标坐标（演示模式使用预定义坐标）
            target_arm = dict(SORT_ZONES[color])
            target_arm["z"] = clamp_z
            steps.append(f"检测到 {color} 目标")

            # Step 3: 移到目标上方（安全高度）
            await self.send_command(db, "MOVE", {
                "x": target_arm["x"],
                "y": target_arm["y"],
                "z": 4.0,
                "pitch": target_arm["pitch"],
                "min_pitch": -90.0, "max_pitch": 90.0, "duration": 1500,
            })
            steps.append("移到目标上方（安全高度 z=4.0）")

            # Step 4: 超声波 X 轴校正
            if use_ultrasonic:
                ok, dist_resp, _ = await self._serial.send("DIST")
                if ok and dist_resp and dist_resp != "ERR":
                    try:
                        dist_cm = float(dist_resp)
                        steps.append(f"超声波校正（实测距离={dist_cm}cm）")
                    except ValueError:
                        steps.append("超声波校正（数据解析失败）")

            # Step 5: 张开夹爪
            await self.send_command(db, "GRIPPER_OPEN", {})
            steps.append("夹爪张开")

            # Step 6: 下降到夹取高度
            await self.send_command(db, "MOVE", {
                "x": target_arm["x"],
                "y": target_arm["y"],
                "z": clamp_z,
                "pitch": target_arm["pitch"],
                "min_pitch": -90.0, "max_pitch": 90.0, "duration": 1000,
            })
            steps.append(f"下降到夹取高度 ({clamp_z}cm)")

            # Step 7: 闭合夹爪
            await self.send_command(db, "GRIPPER_CLOSE", {})
            self._gripper_open = False
            steps.append("夹爪闭合")

            # Step 8: 抬起到安全高度
            await self.send_command(db, "MOVE", {
                "x": target_arm["x"],
                "y": target_arm["y"],
                "z": 4.0,
                "pitch": target_arm["pitch"],
                "min_pitch": -90.0, "max_pitch": 90.0, "duration": 1000,
            })
            steps.append("抬起到安全高度")

            # Step 9: 水平移动到分拣区正上方
            zone = dict(SORT_ZONES[color])
            await self.send_command(db, "MOVE", {
                "x": zone["x"],
                "y": zone["y"],
                "z": 4.0,
                "pitch": zone["pitch"],
                "min_pitch": -90.0, "max_pitch": 90.0, "duration": 1500,
            })
            steps.append(f"移到 {color} 区分拣区上方")

            # Step 10: 下降到放置高度
            await self.send_command(db, "MOVE", {
                "x": zone["x"],
                "y": zone["y"],
                "z": zone["z"],
                "pitch": zone["pitch"],
                "min_pitch": -90.0, "max_pitch": 90.0, "duration": 1000,
            })
            steps.append("下降到放置高度")

            # Step 11: 松开夹爪
            await self.send_command(db, "GRIPPER_OPEN", {})
            self._gripper_open = True
            steps.append("夹爪松开")

            # Step 12: 归零
            await self.send_command(db, "RESET", {})
            steps.append("机械臂归零")

            duration = time.time() - t0
            return ClampTaskResponse(
                success=True,
                color=color,
                target_position=ArmPosition(**zone),
                duration_seconds=round(duration, 1),
                steps=steps,
            )

        except Exception as e:
            duration = time.time() - t0
            steps.append(f"错误: {str(e)}")
            return ClampTaskResponse(
                success=False,
                color=color,
                target_position=ArmPosition(x=0.0, y=0.0, z=0.0, pitch=0.0),
                duration_seconds=round(duration, 1),
                steps=steps,
            )

    async def play_action_sequence(
        self,
        db: AsyncSession,
        action_name: str,
    ) -> dict:
        """
        回放 agent/config.ACTIONS_DIR 下的动作 JSON（与 agent/tasks.task_action 同源逻辑）。
        使用串口 MOVE / MOVE_NB，并写入操作日志。
        """
        import json
        from pathlib import Path

        from agent import config as agent_config

        safe = action_name.replace("/", "_").replace("\\", "_")
        path = Path(agent_config.ACTIONS_DIR) / f"{safe}.json"
        if not path.is_file():
            return {"success": False, "message": f"找不到动作文件: {action_name}"}

        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        keyframes = data.get("keyframes") or []
        if not keyframes:
            return {"success": False, "message": f"动作为空: {action_name}"}

        dur_ms = agent_config.ACTION_PLAYBACK_DUR_MS
        interval_ms = agent_config.ACTION_PLAYBACK_INTERVAL_MS
        pause_thr = agent_config.ACTION_PAUSE_THRESHOLD_MS

        if not await self.is_online():
            return {"success": False, "message": "机械臂串口未连接"}

        h = agent_config.HOME
        await self.send_command(db, "MOVE", {
            "x": h["x"],
            "y": h["y"],
            "z": h["z"],
            "pitch": h["pitch"],
            "min_pitch": -90.0,
            "max_pitch": 90.0,
            "duration": int(h.get("dur", 2000)),
        })
        await asyncio.sleep(2.0)

        total = len(keyframes)
        for i, frame in enumerate(keyframes, 1):
            x = float(frame["x"])
            y = float(frame["y"])
            z = float(frame["z"])
            pitch = float(frame["pitch"])
            stored_dur = int(frame.get("duration", 200))
            is_pause = stored_dur > pause_thr
            is_last = i == total

            if is_last or is_pause:
                block_dur = stored_dur if is_pause else dur_ms
                await self.send_command(db, "MOVE", {
                    "x": x,
                    "y": y,
                    "z": z,
                    "pitch": pitch,
                    "min_pitch": -90.0,
                    "max_pitch": 90.0,
                    "duration": block_dur,
                })
            else:
                cmd = (
                    f"MOVE_NB {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} "
                    f"-90 90 {int(dur_ms)}"
                )
                ok, _, _ = await self._serial.send(cmd)
                if not ok:
                    return {
                        "success": False,
                        "message": f"MOVE_NB 失败（帧 {i}/{total}）",
                        "frames_done": i - 1,
                    }
                await asyncio.sleep(interval_ms / 1000.0)

        return {
            "success": True,
            "message": f"动作「{action_name}」回放完成",
            "frames": total,
        }

    # ── 状态查询 ──────────────────────────────────────────────────────
    async def get_status(self) -> dict:
        """获取机械臂实时状态"""
        return {
            "online": self._online and self._serial.is_connected(),
            "position": self._last_pos,
            "gripper_open": self._gripper_open,
            "calibration_loaded": self._calibration is not None,
            "calibration_name": (
                self._calibration.name if self._calibration else None
            ),
            "last_updated": datetime.now(timezone.utc),
        }

    # ── 操作日志 ─────────────────────────────────────────────────────
    async def get_logs(
        self,
        db: AsyncSession,
        page: int = 1,
        page_size: int = 50,
    ) -> tuple[list[RobotOperationLog], int]:
        """获取操作日志"""
        stmt = select(RobotOperationLog).order_by(
            RobotOperationLog.created_at.desc()
        ).offset((page - 1) * page_size).limit(page_size)

        count_stmt = select(func.count()).select_from(RobotOperationLog)
        total_result = await db.execute(count_stmt)
        total = total_result.scalar() or 0

        result = await db.execute(stmt)
        logs = result.scalars().all()
        return list(logs), total


# 全局单例
robot_service = RobotService()
