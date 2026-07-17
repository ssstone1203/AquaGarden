from fastapi import APIRouter, Depends

from app.core.security import get_current_user
from app.core.config import settings
from app.schemas.common import AiChatRequest
from app.services import ai_service
from app.services.state import state


router = APIRouter(dependencies=[Depends(get_current_user)])


@router.post("/api/ai/ecosystem-analysis")
async def ecosystem_analysis() -> dict:
    snapshot, from_hardware = _current_sensor_context()
    return await ai_service.analyze(snapshot, from_hardware)


@router.post("/api/ai/chat")
async def chat(request: AiChatRequest) -> dict:
    snapshot, from_hardware = _current_sensor_context()
    return await ai_service.chat(snapshot, from_hardware, request.message, request.history)


def _current_sensor_context():
    max_age_ms = settings.sensors_realtime_max_age_ms
    from_hardware = state.has_fresh_hardware_snapshot(max_age_ms)
    return state.read_fresh_or_demo(max_age_ms), from_hardware
