"""
DashScope OpenAI 兼容接口的 httpx 客户端（统一超时、代理）。

无法连接时请在环境变量或 .env 中设置 HTTPS_PROXY（或 HTTP_PROXY）后重启后端。
"""

from __future__ import annotations

import os

import httpx
from openai import OpenAI

import config


def openai_client() -> OpenAI:
    proxy = (
        os.getenv("HTTPS_PROXY")
        or os.getenv("https_proxy")
        or os.getenv("HTTP_PROXY")
        or os.getenv("http_proxy")
    )
    http_client = httpx.Client(
        proxy=proxy or None,
        timeout=httpx.Timeout(120.0, connect=45.0),
        trust_env=True,
    )
    return OpenAI(
        api_key=config.DASHSCOPE_API_KEY,
        base_url=config.DASHSCOPE_BASE_URL,
        http_client=http_client,
    )
