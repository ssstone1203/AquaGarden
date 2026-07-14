from functools import lru_cache
from typing import List

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", env_file_encoding="utf-8", extra="ignore")

    host: str = "0.0.0.0"
    port: int = Field(default=8090, alias="AQUAGARDEN_FASTAPI_PORT")
    reload: bool = Field(default=False, alias="AQUAGARDEN_FASTAPI_RELOAD")
    database_url: str = "sqlite:///./aquagarden.db"

    jwt_secret: str = Field(
        default="your-secret-key-change-in-production-min-256-bits-please-use-long-secret",
        alias="AQUAGARDEN_JWT_SECRET",
    )
    jwt_expiration_minutes: int = Field(default=30, alias="AQUAGARDEN_JWT_EXPIRATION_MINUTES")

    cors_origins: List[str] = ["*"]
    cors_origin_regex: str | None = None

    device_upload_token: str = Field(default="123456789", alias="AQUAGARDEN_DEVICE_UPLOAD_TOKEN")
    sensors_realtime_max_age_ms: int = Field(default=10000, alias="AQUAGARDEN_SENSORS_REALTIME_MAX_AGE_MS")
    sensor_readings_max_pages: int = Field(default=100, alias="AQUAGARDEN_SENSOR_READINGS_MAX_PAGES")
    sensor_readings_page_size: int = Field(default=20, alias="AQUAGARDEN_SENSOR_READINGS_PAGE_SIZE")

    bridge_base_url: str = Field(default="http://192.168.81.50:18080", alias="AQUAGARDEN_BRIDGE_BASE_URL")
    bridge_auth_header_name: str = Field(default="", alias="AQUAGARDEN_BRIDGE_AUTH_HEADER_NAME")
    bridge_auth_header_value: str = Field(default="", alias="AQUAGARDEN_BRIDGE_AUTH_HEADER_VALUE")
    serial_pump_enabled: bool = Field(default=False, alias="AQUAGARDEN_SERIAL_PUMP_ENABLED")
    serial_pump_base_url: str = Field(default="http://127.0.0.1:18080", alias="AQUAGARDEN_SERIAL_PUMP_BASE_URL")
    camera_rgb_url: str = Field(default="", alias="AQUAGARDEN_CAMERA_RGB_URL")
    camera_depth_url: str = Field(default="", alias="AQUAGARDEN_CAMERA_DEPTH_URL")

    hardware_serial_enabled: bool = Field(default=True, alias="AQUAGARDEN_HARDWARE_SERIAL_ENABLED")
    hardware_serial_port: str = Field(default="COM4", alias="AQUAGARDEN_HARDWARE_SERIAL_PORT")
    hardware_serial_baud: int = Field(default=115200, alias="AQUAGARDEN_HARDWARE_SERIAL_BAUD")
    hardware_serial_max_jpeg_bytes: int = Field(default=524288, alias="AQUAGARDEN_HARDWARE_SERIAL_MAX_JPEG_BYTES")
    hardware_serial_persist_interval_ms: int = Field(default=1000, alias="AQUAGARDEN_HARDWARE_SERIAL_PERSIST_INTERVAL_MS")
    hardware_serial_reconnect_delay_ms: int = Field(default=3000, alias="AQUAGARDEN_HARDWARE_SERIAL_RECONNECT_DELAY_MS")

    llm_provider: str = Field(default="anthropic", alias="AQUAGARDEN_LLM_PROVIDER")
    llm_enabled: bool = Field(default=True, alias="AQUAGARDEN_LLM_ENABLED")
    llm_base_url: str = Field(default="https://api.kimi.com/coding", alias="AQUAGARDEN_LLM_BASE_URL")
    llm_api_key: str = Field(default="", alias="ANTHROPIC_API_KEY")
    llm_model: str = Field(default="kimi-for-coding", alias="AQUAGARDEN_LLM_MODEL")
    llm_max_tokens: int = Field(default=768, alias="AQUAGARDEN_LLM_MAX_TOKENS")
    llm_anthropic_version: str = Field(default="2023-06-01", alias="AQUAGARDEN_LLM_ANTHROPIC_VERSION")


@lru_cache
def get_settings() -> Settings:
    return Settings()


settings = get_settings()
