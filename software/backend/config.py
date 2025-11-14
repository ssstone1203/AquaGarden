"""
配置文件
生产环境请修改这些配置或使用环境变量
"""
import os

# JWT配置
SECRET_KEY = os.getenv("SECRET_KEY", "your-secret-key-change-in-production")
ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 30

# 服务器配置
HOST = os.getenv("HOST", "0.0.0.0")
PORT = int(os.getenv("PORT", "8000"))

# 摄像头配置
ROBOT_CAMERA_ID = int(os.getenv("ROBOT_CAMERA_ID", "0"))
TANK_CAMERA_ID = int(os.getenv("TANK_CAMERA_ID", "1"))

# 传感器配置
SENSOR_PORT = os.getenv("SENSOR_PORT", "/dev/ttyUSB0")
SENSOR_BAUDRATE = int(os.getenv("SENSOR_BAUDRATE", "9600"))

# 日志配置
LOG_LEVEL = os.getenv("LOG_LEVEL", "INFO")

