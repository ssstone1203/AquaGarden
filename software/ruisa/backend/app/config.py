"""Ruisa Agent Web 后端 — 最小配置（与 agent/agent.py 同机运行，直连 tasks.py）"""

from pathlib import Path
from typing import List

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict

_BACKEND_ROOT = Path(__file__).resolve().parent.parent
_RUISA_ROOT = _BACKEND_ROOT.parent


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=(_BACKEND_ROOT / ".env", _RUISA_ROOT / ".env", ".env"),
        env_file_encoding="utf-8",
        case_sensitive=False,
        extra="ignore",
    )

    app_env: str = Field(default="development", alias="APP_ENV")
    debug: bool = Field(default=True, alias="DEBUG")
    app_host: str = Field(default="0.0.0.0", alias="APP_HOST")
    app_port: int = Field(default=8000, alias="APP_PORT")
    api_v1_prefix: str = Field(default="/api/v1", alias="API_V1_PREFIX")
    log_level: str = Field(default="INFO", alias="LOG_LEVEL")

    cors_origins: List[str] = Field(
        default=[
            "http://localhost:5500",
            "http://127.0.0.1:5500",
            "http://localhost:8000",
            "http://127.0.0.1:8000",
        ],
        alias="CORS_ORIGINS",
    )

    # 与 agent/agent.py DEBUG=1 一致：须先模拟唤醒再发指令
    agent_debug_keyboard: bool = Field(default=False, alias="AGENT_DEBUG_KEYBOARD")

    @property
    def is_development(self) -> bool:
        return self.app_env.lower() in ("development", "dev", "local")


settings = Settings()
