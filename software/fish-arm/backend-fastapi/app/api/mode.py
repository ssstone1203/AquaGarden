from fastapi import APIRouter, Depends, HTTPException

from app.core.security import get_current_user
from app.schemas.common import ModeRequest
from app.services.logs import hub
from app.services.state import state


router = APIRouter(dependencies=[Depends(get_current_user)])
VALID = {"service", "demo"}


@router.post("/api/mode")
async def set_mode(body: ModeRequest) -> dict:
    if body.mode not in VALID:
        raise HTTPException(status_code=400, detail="Invalid mode")
    state.mode = body.mode
    await hub.broadcast({"type": "system", "message": f"模式切换: {body.mode}"})
    return {"status": "success", "mode": state.mode}


@router.get("/api/mode")
def get_mode() -> dict[str, str]:
    return {"mode": state.mode}
