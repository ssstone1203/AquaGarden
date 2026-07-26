from typing import Any

import httpx

from app.core.config import settings


def _base_url() -> str:
    return settings.bridge_base_url.rstrip("/") or "http://127.0.0.1:18080"


def _headers() -> dict[str, str]:
    if settings.bridge_auth_header_name.strip() and settings.bridge_auth_header_value.strip():
        return {settings.bridge_auth_header_name.strip(): settings.bridge_auth_header_value.strip()}
    return {}


async def bridge_request(method: str, path: str, json: dict[str, Any] | None = None) -> tuple[int, Any]:
    return await request_url(method, _base_url(), path, json=json, headers=_headers(), timeout=8.0)


async def serial_pump_request(method: str, path: str, json: dict[str, Any] | None = None) -> tuple[int, Any]:
    return await request_url(method, settings.serial_pump_base_url.rstrip("/"), path, json=json, headers={}, timeout=5.0)


async def request_url(
    method: str,
    base_url: str,
    path: str,
    json: dict[str, Any] | None = None,
    headers: dict[str, str] | None = None,
    timeout: float = 8.0,
) -> tuple[int, Any]:
    url = base_url + _normalize_path(path)
    try:
        async with httpx.AsyncClient(timeout=timeout, trust_env=False) as client:
            response = await client.request(method, url, json=json, headers=headers or {})
        try:
            body: Any = response.json()
        except ValueError:
            body = response.text
        return response.status_code, body
    except httpx.HTTPError as exc:
        return 503, {
            "ok": False,
            "message": f"无法连接下游设备服务（{url}）：{exc}. 请确认服务已启动且防火墙放行端口。",
        }


async def bridge_status() -> dict[str, Any]:
    status, body = await bridge_request("GET", "/api/status")
    if 200 <= status < 300 and isinstance(body, dict):
        return body
    message = body.get("message") if isinstance(body, dict) else f"Bridge status HTTP {status}"
    return offline_status(str(message))


def offline_status(reason: str | None = None) -> dict[str, Any]:
    return {
        "ok": False,
        "connected": False,
        "busy": False,
        "currentTask": "idle",
        "phase": "offline",
        "railPosition": None,
        "lastError": reason or "Bridge unavailable",
        "camera": {"hasRgb": False, "hasDepth": False},
    }


def _normalize_path(path: str) -> str:
    return path if path.startswith("/") else "/" + path


async def proxy_stream(url: str):
    async with httpx.AsyncClient(timeout=None, trust_env=False) as client:
        async with client.stream("GET", url, headers=_headers()) as response:
            response.raise_for_status()
            async for chunk in response.aiter_bytes():
                yield chunk


def camera_status() -> dict[str, bool]:
    return {
        "hasRgb": bool(settings.camera_rgb_url.strip()),
        "hasDepth": bool(settings.camera_depth_url.strip()),
        "rgbUrlConfigured": bool(settings.camera_rgb_url.strip()),
        "depthUrlConfigured": bool(settings.camera_depth_url.strip()),
    }


def camera_source(mode: str) -> str:
    return settings.camera_depth_url.strip() if mode == "depth" else settings.camera_rgb_url.strip()
