import logging
from typing import Any

import httpx

from app.core.config import settings
from app.schemas.common import AiChatMessage, SensorSnapshot


logger = logging.getLogger(__name__)


async def analyze(snapshot: SensorSnapshot, from_hardware: bool) -> dict[str, Any]:
    if not _llm_configured():
        return _fallback(snapshot, from_hardware, "skipped", "未配置大模型或功能未启用，已使用本地规则分析。")
    prompt = _sensor_context(snapshot, from_hardware)
    system = "你是水族箱与智慧盆栽一体化生态系统的助手。根据后端提供的实时传感器快照，用中文给出简短、可执行的建议，控制在约 200 字内，分「状态」「风险」「建议」三层表述。不得编造未提供的数据，TDS 数值单位为 NTU。"
    return await _call_llm(snapshot, from_hardware, system, prompt, [])


async def chat(snapshot: SensorSnapshot, from_hardware: bool, message: str, history: list[AiChatMessage]) -> dict[str, Any]:
    if not _llm_configured():
        return _fallback(snapshot, from_hardware, "skipped", "未配置大模型或功能未启用，已使用本地规则回复。")
    system = "你是水族箱与智慧盆栽一体化生态系统的对话助手。根据后端提供的传感器快照和最近对话，用中文回答用户问题；建议要具体、可执行。不得编造未提供的数据，TDS 数值单位为 NTU。"
    messages = [{"role": h.role, "content": h.content[:1200]} for h in history[-12:]]
    content = _sensor_context(snapshot, from_hardware) + "\n用户问题：" + message[:600]
    return await _call_llm(snapshot, from_hardware, system, content, messages)


async def _call_llm(snapshot: SensorSnapshot, from_hardware: bool, system: str, user: str, history: list[dict[str, str]]) -> dict[str, Any]:
    provider = settings.llm_provider
    try:
        if provider == "anthropic":
            text = await _call_anthropic(system, user, history)
        else:
            text = await _call_openai(system, user, history)
        if text:
            return {
                "ok": True,
                "source": "llm",
                "provider": provider,
                "model": settings.llm_model,
                "analysis": text.strip(),
                "llmOk": True,
                "llmStatus": "ok",
                "llmMessage": "大模型已成功返回内容。",
                **_sensor_metadata(snapshot, from_hardware),
            }
        return _fallback(snapshot, from_hardware, "error", "大模型返回为空，已使用本地规则回退。")
    except Exception as exc:
        logger.warning("LLM request failed for provider=%s model=%s error_type=%s", provider, settings.llm_model, exc.__class__.__name__)
        return _fallback(snapshot, from_hardware, "error", "大模型暂时不可用，已使用本地规则回退。")


async def _call_anthropic(system: str, user: str, history: list[dict[str, str]]) -> str:
    endpoint = settings.llm_base_url.rstrip("/")
    endpoint = endpoint + "/messages" if endpoint.endswith("/v1") else endpoint + "/v1/messages"
    body = {
        "model": settings.llm_model,
        "max_tokens": settings.llm_max_tokens,
        "temperature": 0.45,
        "system": system,
        "messages": history + [{"role": "user", "content": user}],
    }
    async with httpx.AsyncClient(timeout=60.0) as client:
        response = await client.post(endpoint, json=body, headers=_anthropic_headers())
    response.raise_for_status()
    data = response.json()
    for block in data.get("content", []):
        if block.get("type") == "text" and block.get("text"):
            return str(block["text"])
    return ""


def _anthropic_headers() -> dict[str, str]:
    headers = {
        "Content-Type": "application/json",
        "anthropic-version": settings.llm_anthropic_version,
    }
    api_key = _llm_api_key()
    if settings.llm_auth_mode == "x-api-key":
        headers["x-api-key"] = api_key
    else:
        headers["Authorization"] = f"Bearer {api_key}"
    return headers


async def _call_openai(system: str, user: str, history: list[dict[str, str]]) -> str:
    endpoint = settings.llm_base_url.rstrip("/") + "/chat/completions"
    messages = [{"role": "system", "content": system}] + history + [{"role": "user", "content": user}]
    headers = {"Content-Type": "application/json", "Authorization": f"Bearer {_llm_api_key()}"}
    body = {
        "model": settings.llm_model,
        "messages": messages,
        "temperature": 0.45,
        "max_tokens": settings.llm_max_tokens,
        "stream": False,
    }
    async with httpx.AsyncClient(timeout=45.0) as client:
        response = await client.post(endpoint, json=body, headers=headers)
    response.raise_for_status()
    return response.json().get("choices", [{}])[0].get("message", {}).get("content", "")


def _llm_configured() -> bool:
    return settings.llm_enabled and bool(_llm_api_key().strip())


def _llm_api_key() -> str:
    return settings.llm_api_key.get_secret_value()


def _fallback(snapshot: SensorSnapshot, from_hardware: bool, status: str, message: str) -> dict[str, Any]:
    return {
        "ok": True,
        "source": "fallback",
        "provider": "none",
        "model": "rule-based",
        "analysis": _rule_based(snapshot, from_hardware),
        "llmOk": False,
        "llmStatus": status,
        "llmMessage": message,
        **_sensor_metadata(snapshot, from_hardware),
    }


def _sensor_context(s: SensorSnapshot, from_hardware: bool) -> str:
    source = "硬件实时采样" if from_hardware else "演示/默认值"
    return f"数据来源：{source}\n水温 {s.water_temp:.1f} °C，气温 {s.air_temp:.1f} °C，空气湿度 {s.air_humidity:.1f} %RH，TDS 水质浊度 {s.wqi:.0f} NTU，土壤湿度 {s.soil_moisture:.0f} %。"


def _sensor_metadata(snapshot: SensorSnapshot, from_hardware: bool) -> dict[str, Any]:
    return {
        "sensorSource": "hardware" if from_hardware else "demo",
        "sensorSnapshot": snapshot.model_dump(),
    }


def _rule_based(s: SensorSnapshot, from_hardware: bool) -> str:
    parts = ["【数据来源】当前为硬件采样。" if from_hardware else "【数据来源】设备未上报或处于演示模式，以下为基于典型阈值的参考判断。"]
    parts.append("【状态】水温偏低，多数热带鱼代谢会放缓。" if s.water_temp < 18 else "【状态】水温偏高，溶氧下降与致病菌风险上升。" if s.water_temp > 30 else "【状态】水温处于常见观赏鱼舒适区间。")
    parts.append("【风险】水体浑浊度偏高，需检查过滤、投喂量并评估换水。" if s.wqi > 1000 else "【风险】水体浑浊度有所升高，建议继续观察趋势。" if s.wqi > 300 else "【风险】当前浑浊度较低，继续保持过滤与定期维护。")
    parts.append("【建议】基质偏干，可适度补水或检查鱼缸废水灌溉链路。" if s.soil_moisture < 35 else "【建议】基质过湿，注意根系透气与霉菌风险。" if s.soil_moisture > 85 else "【建议】基质湿度适中，可按日程 light 投喂与机械臂维护。")
    parts.append("【建议】若长期显示演示数据，请检查 FastAPI 的 COM 串口连接和 MCU 上行帧。")
    return "".join(parts)
