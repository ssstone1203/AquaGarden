from fastapi import APIRouter, Depends

from app.core.security import get_current_user
from app.schemas.common import AiChatRequest
from app.services import ai_service
from app.services.state import state


router = APIRouter(dependencies=[Depends(get_current_user)])


@router.post("/api/ai/ecosystem-analysis")
async def ecosystem_analysis() -> dict:
    snapshot = state.read_sensors()
    return await ai_service.analyze(snapshot, state.has_hardware_snapshot())


@router.post("/api/ai/chat")
async def chat(request: AiChatRequest) -> dict:
    snapshot = state.read_sensors()
    return await ai_service.chat(snapshot, state.has_hardware_snapshot(), request.message, request.history)
