from pydantic import BaseModel, Field, StrictBool, StrictInt


class SensorSnapshot(BaseModel):
    water_temp: float = 24.0
    air_temp: float = 26.0
    air_humidity: float = 55.0
    wqi: float = 70.0
    soil_moisture: float = 62.0


class RobotControlRequest(BaseModel):
    direction: str = Field(min_length=1)


class ModeRequest(BaseModel):
    mode: str = Field(min_length=1)


class AtomizerControlRequest(BaseModel):
    state: StrictBool


class UsbLightControlRequest(BaseModel):
    mode: StrictInt = Field(ge=0, le=26)


class AiChatMessage(BaseModel):
    role: str = Field(pattern="^(user|assistant)$")
    content: str = Field(min_length=1, max_length=6000)


class AiChatRequest(BaseModel):
    message: str = Field(min_length=1, max_length=600)
    history: list[AiChatMessage] = Field(default_factory=list, max_length=12)


class Detection(BaseModel):
    label: str = "object"
    x: float = 0
    y: float = 0
    width: float | None = None
    w: float | None = None
    height: float | None = None
    h: float | None = None
    score: float | None = None
    confidence: float | None = None


class DetectionPayload(BaseModel):
    detections: list[Detection]
