"""
arm.py  ——  共享串口 Arm 类
所有任务通过同一个 Arm 实例操作机械臂，避免多次开关串口。

连接方式：pyserial.Serial(port, baud)；Windows 端口名为 COMx（如 COM3），
与 config.SERIAL_PORT / 环境变量 SERIAL_PORT 一致，默认波特率 config.SERIAL_BAUD（通常 115200）。
"""

import math
import threading
import time
from typing import Optional

try:
    from serial import Serial, SerialException
except ImportError:
    raise ImportError(
        "未安装 pyserial 或误装了 PyPI 上的同名包 'serial'。"
        "请执行: pip uninstall serial -y; pip install pyserial"
    ) from None

import config


class Arm:
    """线程安全的机械臂串口封装"""

    def __init__(self, port: Optional[str] = None, baud: Optional[int] = None):
        port = port if port is not None else config.SERIAL_PORT
        baud = baud if baud is not None else getattr(config, "SERIAL_BAUD", 115200)
        try:
            self._s = Serial(port, baud, timeout=0)
        except SerialException as e:
            raise SerialException(
                f"无法打开串口 {port!r}（波特率 {baud}）。请检查：USB 是否插好、驱动是否正常、"
                f"设备管理器中 COM 号是否变化；可在环境变量或 .env 中设置 SERIAL_PORT=COMx "
                f"（Linux 一般为 /dev/ttyUSB0），必要时设置 SERIAL_BAUD。"
            ) from e
        self._lock = threading.Lock()
        self._buf = b""
        time.sleep(0.3)
        self._s.reset_input_buffer()

    # ── 底层通信 ──────────────────────────────────────────────────
    def cmd(self, c: str, timeout: float = 10.0) -> str:
        with self._lock:
            self._s.write((c + "\n").encode())
            return self._readline(timeout)

    def _readline(self, timeout: float) -> str:
        t0 = time.time()
        while time.time() - t0 < timeout:
            if self._s.in_waiting:
                self._buf += self._s.read(self._s.in_waiting)
            if b"\n" in self._buf:
                line, self._buf = self._buf.split(b"\n", 1)
                return line.decode(errors="ignore").strip()
            time.sleep(0.01)
        return ""

    # ── 运动控制 ──────────────────────────────────────────────────
    def move(self, x, y, z, pitch, dur=1000, mn=-90, mx=90) -> bool:
        return self.cmd(
            f"MOVE {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} {mn} {mx} {int(dur)}",
            timeout=dur / 1000 + 5,
        ) == "OK"

    def move_nb(self, x, y, z, pitch, dur=250) -> bool:
        """非阻塞移动（约 120ms 后返回 OK）"""
        return self.cmd(
            f"MOVE_NB {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} -90 90 {int(dur)}",
            timeout=3,
        ) == "OK"

    def move_angle(self, angle_deg, z, pitch, r=config.FACE_R, dur=400) -> bool:
        """以底座角度控制（人脸追踪用）"""
        angle_deg = max(config.FACE_ANGLE_MIN, min(config.FACE_ANGLE_MAX, angle_deg))
        rad = math.radians(angle_deg)
        x = r * math.cos(rad)
        y = r * math.sin(rad)
        return self.move(x, y, z, pitch, dur=dur)

    def go_home(self) -> bool:
        h = config.HOME
        return self.move(h["x"], h["y"], h["z"], h["pitch"], dur=h["dur"])

    # ── 夹爪 ─────────────────────────────────────────────────────
    def gripper_open(self, dur=600) -> bool:
        return self.cmd(f"GRIPPER_CLOSE {dur}", timeout=dur / 1000 + 3) == "OK"

    def gripper_close(self, dur=600) -> bool:
        return self.cmd(f"GRIPPER_OPEN {dur}", timeout=dur / 1000 + 3) == "OK"

    # ── 传感器 ───────────────────────────────────────────────────
    def dist(self) -> float:
        """超声波测距，失败返回 -1.0"""
        r = self.cmd("DIST", 3)
        try:
            v = float(r)
            return v if v > 0 else -1.0
        except Exception:
            return -1.0

    # ── LED ──────────────────────────────────────────────────────
    def led(self, r: int, g: int, b: int, bright: int = 128):
        self.cmd(f"LED_BRIGHT {bright}")
        self.cmd(f"LED_ALL {r} {g} {b}")

    def led_off(self):
        self.cmd("LED_ALL 0 0 0")

    # ── 生命周期 ─────────────────────────────────────────────────
    def ping(self) -> bool:
        return self.cmd("PING", 2) == "PONG"

    def close(self):
        if self._s and getattr(self._s, "is_open", False):
            self._s.close()
