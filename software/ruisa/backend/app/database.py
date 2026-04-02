"""
数据库连接管理
使用 SQLAlchemy 2.0 异步引擎
"""
from contextlib import asynccontextmanager
from pathlib import Path
from typing import AsyncGenerator, Optional
from urllib.parse import unquote

from sqlalchemy.ext.asyncio import (
    AsyncSession,
    async_sessionmaker,
    create_async_engine,
)
from sqlalchemy.orm import DeclarativeBase

from app.config import settings

# backend 根目录（app/ 的上一级），用于解析相对路径的 SQLite 文件
BACKEND_ROOT = Path(__file__).resolve().parent.parent


def normalize_sqlite_url(url: str) -> str:
    """
    相对路径的 sqlite URL（如 ./data/aquagarden.db）会随启动目录变化而失败。
    统一解析为 backend 目录下的绝对路径，并创建所在文件夹。
    """
    if ":memory:" in url.lower():
        return url
    lower = url.lower()
    if "sqlite" not in lower:
        return url

    raw: Optional[str] = None
    for prefix in ("sqlite+aiosqlite:///", "sqlite:///"):
        if url.startswith(prefix):
            raw = unquote(url[len(prefix) :].split("?")[0])
            break
    if raw is None:
        return url

    path = Path(raw)
    # URL 中可能出现的 /C:/... 形式
    if raw.startswith("/") and len(raw) >= 3 and raw[2] == ":":
        path = Path(raw[1:])
    if not path.is_absolute():
        path = (BACKEND_ROOT / path).resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    return f"sqlite+aiosqlite:///{path.as_posix()}"


# ── 异步引擎 ────────────────────────────────────────────────────────────────
engine = create_async_engine(
    normalize_sqlite_url(settings.database_url),
    echo=settings.debug,
)

# ── 异步会话工厂 ─────────────────────────────────────────────────────────────
async_session_maker = async_sessionmaker(
    engine,
    class_=AsyncSession,
    expire_on_commit=False,
    autoflush=False,
    autocommit=False,
)


# ── ORM 基类 ────────────────────────────────────────────────────────────────
class Base(DeclarativeBase):
    """所有模型的基类"""
    pass


# ── 会话依赖 ────────────────────────────────────────────────────────────────
async def get_db() -> AsyncGenerator[AsyncSession, None]:
    """FastAPI 依赖：获取数据库会话，用 yield 确保正确关闭"""
    async with async_session_maker() as session:
        try:
            yield session
            await session.commit()
        except Exception:
            await session.rollback()
            raise
        finally:
            await session.close()


# ── 上下文管理器会话 ─────────────────────────────────────────────────────────
@asynccontextmanager
async def get_db_context() -> AsyncGenerator[AsyncSession, None]:
    """非 FastAPI 场景下获取数据库会话（如 Celery 任务）"""
    async with async_session_maker() as session:
        try:
            yield session
            await session.commit()
        except Exception:
            await session.rollback()
            raise


# ── 初始化建表（开发用）───────────────────────────────────────────────────────
async def init_db() -> None:
    """在开发环境创建所有表"""
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
