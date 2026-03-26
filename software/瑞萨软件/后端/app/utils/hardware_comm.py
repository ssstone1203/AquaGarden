"""
RA6M5 串口通信工具
封装与机械臂固件的串口协议通信
"""
import asyncio
import struct
from typing import Optional

from app.config import settings


class HardwareComm:
    """
    RA6M5 串口协议通信封装。
    当前为演示模式 stub，生产环境需实现实际串口通信。
    """

    _serial_port: Optional[object] = None
    _lock: asyncio.Lock = asyncio.Lock()

    @classmethod
    async def init(cls) -> bool:
        """初始化串口连接"""
        try:
            import serial
            cls._serial_port = serial.Serial(
                port=settings.robot_serial_port,
                baudrate=settings.robot_baudrate,
                timeout=3.0,
            )
            return True
        except Exception:
            cls._serial_port = None
            return False

    @classmethod
    async def close(cls) -> None:
        """关闭串口连接"""
        if cls._serial_port:
            try:
                cls._serial_port.close()
            except Exception:
                pass
            cls._serial_port = None

    @classmethod
    async def send_command(
        cls,
        cmd: str,
        timeout: float = 3.0,
    ) -> tuple[bool, str]:
        """发送命令到 RA6M5。返回 (success, response_string)"""
        async with cls._lock:
            try:
                if cls._serial_port is None:
                    return False, "SERIAL_NOT_CONNECTED"

                cls._serial_port.write(f"{cmd}\n".encode())
                response = cls._serial_port.readline().decode().strip()
                return response == "OK", response

            except asyncio.TimeoutError:
                return False, "TIMEOUT"
            except Exception as e:
                return False, str(e)

    @classmethod
    async def is_connected(cls) -> bool:
        """检查串口连接状态"""
        return cls._serial_port is not None
