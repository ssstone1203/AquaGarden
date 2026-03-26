"""
Redis 连接管理
"""
from typing import AsyncGenerator, Optional

import redis.asyncio as redis
from redis.asyncio import Redis

from app.config import settings


class RedisManager:
    """Redis 连接池管理器"""

    _pool: Optional[redis.ConnectionPool] = None
    _client: Optional[Redis] = None

    @classmethod
    async def init(cls) -> None:
        """初始化 Redis 连接池"""
        cls._pool = redis.ConnectionPool.from_url(
            settings.redis_url,
            password=settings.redis_password or None,
            max_connections=50,
            decode_responses=True,
        )
        cls._client = redis.Redis(connection_pool=cls._pool)

    @classmethod
    async def close(cls) -> None:
        """关闭连接池"""
        if cls._client:
            await cls._client.close()
        if cls._pool:
            await cls._pool.disconnect()

    @classmethod
    def client(cls) -> Redis:
        """获取 Redis 客户端"""
        if cls._client is None:
            raise RuntimeError("Redis has not been initialized. Call RedisManager.init() first.")
        return cls._client

    @classmethod
    async def ping(cls) -> bool:
        """检查 Redis 连接"""
        try:
            return await cls._client.ping()
        except Exception:
            return False


async def get_redis() -> AsyncGenerator[Redis, None]:
    """FastAPI 依赖：获取 Redis 客户端"""
    yield RedisManager.client()
