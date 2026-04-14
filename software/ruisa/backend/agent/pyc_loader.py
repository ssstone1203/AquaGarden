"""
从本地 .pyc 加载扩展模块（与运行时的 Python 主版本一致，如 3.9 → *cpython-39.pyc）。

说明：
- 仅适合「单文件」扩展；多模块请打成包或使用源码。
- 文件名通常为「模块名.cpython-XY.pyc」，不能带空格或「copy」等随意后缀。
- 放置位置示例：agent/__pycache__/intent_engine.cpython-39.pyc
"""

from __future__ import annotations

import logging
from importlib.machinery import SourcelessFileLoader
from importlib.util import module_from_spec, spec_from_loader
from pathlib import Path
from types import ModuleType
from typing import Optional

logger = logging.getLogger(__name__)


def load_pyc_module(
    path: Path,
    module_name: str = "_ruisa_agent_trained",
) -> Optional[ModuleType]:
    p = Path(path).expanduser().resolve()
    if not p.is_file():
        logger.warning("[Agent] .pyc 文件不存在: %s", p)
        return None
    try:
        loader = SourcelessFileLoader(module_name, str(p))
        spec = spec_from_loader(module_name, loader)
        if spec is None:
            return None
        mod = module_from_spec(spec)
        loader.exec_module(mod)
        logger.info("[Agent] 已从 .pyc 加载扩展: %s", p)
        return mod
    except Exception as e:
        logger.error("[Agent] .pyc 加载失败 %s: %s", p, e, exc_info=True)
        return None
