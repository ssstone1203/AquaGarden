"""
Agent HTTP API：状态与 .pyc 扩展推理（前端可调用）
"""
from fastapi import APIRouter, HTTPException, status

from app.deps import CurrentUser
from app.schemas.agent import TrainedInferRequest
from app.services.agent_service import agent_service

router = APIRouter()


@router.get("/trained/status")
async def trained_status(current_user: CurrentUser):
    """是否已配置并成功加载 AGENT_TRAINED_PYC。"""
    mod = agent_service.get_trained_module()
    p = agent_service.get_trained_pyc_path()
    return {
        "loaded": mod is not None,
        "path": str(p) if p is not None else None,
        "has_parse_intent": mod is not None and callable(getattr(mod, "parse_intent", None)),
        "has_infer": mod is not None and (
            callable(getattr(mod, "infer", None)) or callable(getattr(mod, "predict", None))
        ),
    }


@router.post("/trained/infer")
async def trained_infer(body: TrainedInferRequest, current_user: CurrentUser):
    """
    调用扩展模块的 infer(text, **context) 或 predict(...)。
    result 类型由扩展自行决定（须可 JSON 序列化或为基础类型）。
    """
    mod = agent_service.get_trained_module()
    if mod is None:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="未配置 AGENT_TRAINED_PYC 或加载失败，请检查路径与 Python 版本是否匹配",
        )
    fn = getattr(mod, "infer", None)
    if not callable(fn):
        fn = getattr(mod, "predict", None)
    if not callable(fn):
        raise HTTPException(
            status_code=status.HTTP_501_NOT_IMPLEMENTED,
            detail="扩展模块未实现 infer(text, **kwargs) 或 predict(...)",
        )
    try:
        raw = fn(body.text, **(body.context or {}))
        if hasattr(raw, "model_dump"):
            raw = raw.model_dump()
        return {"ok": True, "result": raw}
    except TypeError:
        # 部分模型仅接收单参数
        try:
            raw = fn(body.text)
            if hasattr(raw, "model_dump"):
                raw = raw.model_dump()
            return {"ok": True, "result": raw}
        except Exception as e:
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail=str(e),
            ) from e
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=str(e),
        ) from e
