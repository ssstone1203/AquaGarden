from functools import lru_cache
from typing import List, Literal

from pydantic import AliasChoices, Field, SecretStr
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", env_file_encoding="utf-8", extra="ignore")

    host: str = "0.0.0.0"
    port: int = Field(default=8090, alias="AQUAGARDEN_FASTAPI_PORT")
    reload: bool = Field(default=False, alias="AQUAGARDEN_FASTAPI_RELOAD")
    database_url: str = Field(default="sqlite:///./aquagarden.db", alias="AQUAGARDEN_DATABASE_URL")

    jwt_secret: str = Field(
        default="your-secret-key-change-in-production-min-256-bits-please-use-long-secret",
        alias="AQUAGARDEN_JWT_SECRET",
    )
    jwt_expiration_minutes: int = Field(default=30, alias="AQUAGARDEN_JWT_EXPIRATION_MINUTES")
    initial_admin_username: str = Field(default="", alias="AQUAGARDEN_ADMIN_USERNAME")
    initial_admin_password: SecretStr = Field(default=SecretStr(""), alias="AQUAGARDEN_ADMIN_PASSWORD")
    initial_admin_email: str | None = Field(default=None, alias="AQUAGARDEN_ADMIN_EMAIL")

    cors_origins: List[str] = ["*"]
    cors_origin_regex: str | None = None

    device_upload_token: str = Field(default="123456789", alias="AQUAGARDEN_DEVICE_UPLOAD_TOKEN")
    sensors_realtime_max_age_ms: int = Field(default=10000, alias="AQUAGARDEN_SENSORS_REALTIME_MAX_AGE_MS")
    sensor_readings_max_pages: int = Field(default=100, alias="AQUAGARDEN_SENSOR_READINGS_MAX_PAGES")
    sensor_readings_page_size: int = Field(default=20, alias="AQUAGARDEN_SENSOR_READINGS_PAGE_SIZE")

    bridge_base_url: str = Field(default="http://10.213.133.50:18080", alias="AQUAGARDEN_BRIDGE_BASE_URL")
    bridge_auth_header_name: str = Field(default="", alias="AQUAGARDEN_BRIDGE_AUTH_HEADER_NAME")
    bridge_auth_header_value: str = Field(default="", alias="AQUAGARDEN_BRIDGE_AUTH_HEADER_VALUE")
    serial_pump_enabled: bool = Field(default=False, alias="AQUAGARDEN_SERIAL_PUMP_ENABLED")
    serial_pump_base_url: str = Field(default="http://127.0.0.1:18080", alias="AQUAGARDEN_SERIAL_PUMP_BASE_URL")
    camera_rgb_url: str = Field(default="", alias="AQUAGARDEN_CAMERA_RGB_URL")
    camera_depth_url: str = Field(default="", alias="AQUAGARDEN_CAMERA_DEPTH_URL")
    raspberry_pi_camera_url: str = Field(
        default="http://10.213.133.50:18080/video/rgb.mjpg",
        alias="AQUAGARDEN_RASPBERRY_PI_CAMERA_URL",
    )
    raspberry_pi_camera_connect_timeout_seconds: float = Field(
        default=3.0,
        alias="AQUAGARDEN_RASPBERRY_PI_CAMERA_CONNECT_TIMEOUT_SECONDS",
    )
    raspberry_pi_camera_reconnect_delay_seconds: float = Field(
        default=2.0,
        alias="AQUAGARDEN_RASPBERRY_PI_CAMERA_RECONNECT_DELAY_SECONDS",
    )

    tank_usb_camera_enabled: bool = Field(default=True, alias="AQUAGARDEN_TANK_USB_CAMERA_ENABLED")
    tank_usb_camera_index: int = Field(default=1, alias="AQUAGARDEN_TANK_USB_CAMERA_INDEX")
    tank_usb_camera_width: int = Field(default=640, alias="AQUAGARDEN_TANK_USB_CAMERA_WIDTH")
    tank_usb_camera_height: int = Field(default=480, alias="AQUAGARDEN_TANK_USB_CAMERA_HEIGHT")
    tank_usb_camera_fps: float = Field(default=10.0, alias="AQUAGARDEN_TANK_USB_CAMERA_FPS")
    tank_usb_camera_jpeg_quality: int = Field(default=80, alias="AQUAGARDEN_TANK_USB_CAMERA_JPEG_QUALITY")
    tank_usb_camera_reconnect_delay_seconds: float = Field(
        default=3.0,
        alias="AQUAGARDEN_TANK_USB_CAMERA_RECONNECT_DELAY_SECONDS",
    )
    tank_video_frame_max_age_ms: int = Field(default=5000, alias="AQUAGARDEN_TANK_VIDEO_FRAME_MAX_AGE_MS")
    tank_yolo_enabled: bool = Field(default=True, alias="AQUAGARDEN_TANK_YOLO_ENABLED")
    tank_yolo_weights_path: str = Field(
        default="model/yolo_fish/runs/yolo11n_fish_new/weights/best.pt",
        alias="AQUAGARDEN_TANK_YOLO_WEIGHTS_PATH",
    )
    tank_yolo_confidence: float = Field(default=0.25, alias="AQUAGARDEN_TANK_YOLO_CONFIDENCE")
    tank_yolo_image_size: int = Field(default=640, alias="AQUAGARDEN_TANK_YOLO_IMAGE_SIZE")
    tank_yolo_every_n_frames: int = Field(default=3, alias="AQUAGARDEN_TANK_YOLO_EVERY_N_FRAMES")
    tank_yolo_device: str = Field(default="", alias="AQUAGARDEN_TANK_YOLO_DEVICE")

    hardware_serial_enabled: bool = Field(default=True, alias="AQUAGARDEN_HARDWARE_SERIAL_ENABLED")
    hardware_serial_port: str = Field(default="COM20", alias="AQUAGARDEN_HARDWARE_SERIAL_PORT")
    hardware_serial_baud: int = Field(default=115200, alias="AQUAGARDEN_HARDWARE_SERIAL_BAUD")
    hardware_serial_max_jpeg_bytes: int = Field(default=524288, alias="AQUAGARDEN_HARDWARE_SERIAL_MAX_JPEG_BYTES")
    hardware_serial_persist_interval_ms: int = Field(default=1000, alias="AQUAGARDEN_HARDWARE_SERIAL_PERSIST_INTERVAL_MS")
    hardware_serial_reconnect_delay_ms: int = Field(default=3000, alias="AQUAGARDEN_HARDWARE_SERIAL_RECONNECT_DELAY_MS")

    llm_provider: Literal["anthropic", "openai"] = Field(default="anthropic", alias="AQUAGARDEN_LLM_PROVIDER")
    llm_enabled: bool = Field(default=True, alias="AQUAGARDEN_LLM_ENABLED")
    llm_base_url: str = Field(default="https://api.kimi.com/coding", alias="AQUAGARDEN_LLM_BASE_URL")
    llm_auth_mode: Literal["bearer", "x-api-key"] = Field(
        default="bearer",
        alias="AQUAGARDEN_LLM_AUTH_MODE",
    )
    llm_api_key: SecretStr = Field(
        default=SecretStr(""),
        validation_alias=AliasChoices(
            "AQUAGARDEN_LLM_API_KEY",
            "ANTHROPIC_AUTH_TOKEN",
            "ANTHROPIC_API_KEY",
            "OPENAI_API_KEY",
        ),
    )
    llm_model: str = Field(default="kimi-for-coding", alias="AQUAGARDEN_LLM_MODEL")
    llm_max_tokens: int = Field(default=768, alias="AQUAGARDEN_LLM_MAX_TOKENS")
    llm_anthropic_version: str = Field(default="2023-06-01", alias="AQUAGARDEN_LLM_ANTHROPIC_VERSION")


@lru_cache
def get_settings() -> Settings:
    return Settings()


settings = get_settings()
