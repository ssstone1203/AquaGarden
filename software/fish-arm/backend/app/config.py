"""
Configuration settings for AquaGarden using pydantic-settings.
"""
from pathlib import Path
from typing import List

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict

_BACKEND_ROOT = Path(__file__).resolve().parent.parent
_AQUAGARDEN_ROOT = _BACKEND_ROOT.parent

class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=(_BACKEND_ROOT / ".env", _AQUAGARDEN_ROOT / ".env", ".env"),
        env_file_encoding="utf-8",
        case_sensitive=False,
        extra="ignore",
    )

    app_env: str = Field(default="development", alias="APP_ENV")
    debug: bool = Field(default=True, alias="DEBUG")
    app_host: str = Field(default="0.0.0.0", alias="APP_HOST")
    app_port: int = Field(default=8090, alias="APP_PORT")
    api_v1_prefix: str = Field(default="/api", alias="API_V1_PREFIX")
    log_level: str = Field(default="INFO", alias="LOG_LEVEL")
    
    # JWT settings
    secret_key: str = Field(
        default="your-secret-key-change-in-production",
        alias="SECRET_KEY"
    )
    algorithm: str = Field(default="HS256", alias="ALGORITHM")
    access_token_expire_minutes: int = Field(default=30, alias="ACCESS_TOKEN_EXPIRE_MINUTES")
    
    # AquaGarden specific
    max_log_entries: int = Field(default=1000, alias="MAX_LOG_ENTRIES")
    sensor_update_interval: int = Field(default=3, alias="SENSOR_UPDATE_INTERVAL")
    websocket_ping_interval: int = Field(default=30, alias="WEBSOCKET_PING_INTERVAL")
    database_url: str = Field(
        default="sqlite:///./aquagarden.db", 
        alias="DATABASE_URL"
    )
    
    cors_origins: List[str] = Field(
        default=["*"],
        alias="CORS_ORIGINS",
    )

    @property
    def is_development(self) -> bool:
        return self.app_env.lower() in ("development", "dev", "local")


settings = Settings()
