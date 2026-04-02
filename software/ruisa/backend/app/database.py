"""
数据库连接管理
使用 SQLAlchemy 2.0 异步引擎
"""
from contextlib import asynccontextmanager
from typing import AsyncGenerator

from sqlalchemy.ext.asyncio import (
    AsyncSession,
    async_sessionmaker,
    create_async_engine,
)
from sqlalchemy.orm import DeclarativeBase

from app.config import settings


# ── 异步引擎 ────────────────────────────────────────────────────────────────
engine = create_async_engine(
    settings.database_url,
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
