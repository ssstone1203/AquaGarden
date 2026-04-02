"""
应用配置管理
仅保留机械臂控制相关配置
"""
from pathlib import Path
from typing import List, Optional

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict

# backend/ 与 ruisa/.env：无论从哪级目录启动都能找到配置
_BACKEND_ROOT = Path(__file__).resolve().parent.parent
_RUISA_ROOT = _BACKEND_ROOT.parent
# AquaGarden 仓库根（ruisa → software → 项目根），与 agent/config.py MAP_FILE 一致
_REPO_ROOT = _RUISA_ROOT.parent.parent
_DEFAULT_TEACH_MAP = _REPO_ROOT / "model" / "calibration" / "teach_map.npz"


class Settings(BaseSettings):
    """全局配置"""

    model_config = SettingsConfigDict(
        env_file=(
            _BACKEND_ROOT / ".env",
            _RUISA_ROOT / ".env",
            ".env",
        ),
        env_file_encoding="utf-8",
        case_sensitive=False,
        extra="ignore",
    )

    # ── 数据库 ──
    database_url: str = Field(
        default="sqlite+aiosqlite:///./data/aquagarden.db",
        alias="DATABASE_URL",
    )

    # ── Redis ──
    redis_url: str = Field(default="redis://localhost:6379/0", alias="REDIS_URL")
    redis_password: str = Field(default="", alias="REDIS_PASSWORD")

    # ── JWT ──
    jwt_secret_key: str = Field(
        default="change-me-in-production",
        alias="JWT_SECRET_KEY",
    )
    jwt_algorithm: str = Field(default="HS256", alias="JWT_ALGORITHM")
    access_token_expire_minutes: int = Field(
        default=120, alias="ACCESS_TOKEN_EXPIRE_MINUTES"
    )
    refresh_token_expire_days: int = Field(
        default=7, alias="REFRESH_TOKEN_EXPIRE_DAYS"
    )

    # ── 应用 ──
    app_env: str = Field(default="development", alias="APP_ENV")
    debug: bool = Field(default=True, alias="DEBUG")
    app_host: str = Field(default="0.0.0.0", alias="APP_HOST")
    app_port: int = Field(default=8000, alias="APP_PORT")
    api_v1_prefix: str = Field(default="/api/v1", alias="API_V1_PREFIX")

    # ── CORS ──
    cors_origins: List[str] = Field(
        default=[
            "http://localhost:3000",
            "http://localhost:5500",
            "http://127.0.0.1:5500",
            "http://localhost:8000",
            "http://127.0.0.1:8000",
        ],
        alias="CORS_ORIGINS",
    )

    # ── 机械臂硬件（RA6M5 串口协议）────────────────────────────────────────
    robot_serial_port: str = Field(
        default="/dev/ttyUSB0",
        alias="ROBOT_SERIAL_PORT",
    )
    robot_baudrate: int = Field(default=115200, alias="ROBOT_BAUDRATE")

    # 与 software/ruisa/agent/config.py MAP_FILE、target/clamp/collect_teach.py 输出路径一致
    robot_teach_map_path: Path = Field(
        default=_DEFAULT_TEACH_MAP,
        alias="ROBOT_TEACH_MAP_PATH",
    )

    # Agent 单文件 .pyc 扩展（须与后端 Python 版本一致）；见 agent/pyc_loader.py
    agent_trained_pyc: Optional[Path] = Field(default=None, alias="AGENT_TRAINED_PYC")

    # 与 agent/agent.py DEBUG=1 一致：需先「模拟唤醒」再处理对话（Web 端用按钮/专用消息替代 Enter）
    agent_debug_keyboard: bool = Field(default=False, alias="AGENT_DEBUG_KEYBOARD")

    # ── AI 模型路径 ───────────────────────────────────────────────────────
    model_path_yolo: Path = Field(
        default=Path("/models/yolov8n.pt"), alias="MODEL_PATH_YOLO"
    )

    # ── 文件存储（MinIO / S3）────────────────────────────────────────────
    minio_endpoint: str = Field(default="localhost:9000", alias="MINIO_ENDPOINT")
    minio_access_key: str = Field(default="minioadmin", alias="MINIO_ACCESS_KEY")
    minio_secret_key: str = Field(default="minioadmin", alias="MINIO_SECRET_KEY")
    minio_secure: bool = Field(default=False, alias="MINIO_SECURE")

    # ── Celery ──
    celery_broker_url: str = Field(
        default="redis://localhost:6379/1", alias="CELERY_BROKER_URL"
    )

    # ── 日志 ──
    log_level: str = Field(default="INFO", alias="LOG_LEVEL")

    # ── 派生属性 ──
    @property
    def is_development(self) -> bool:
        return self.app_env == "development"

    @property
    def is_production(self) -> bool:
        return self.app_env == "production"


# 全局单例
settings = Settings()
