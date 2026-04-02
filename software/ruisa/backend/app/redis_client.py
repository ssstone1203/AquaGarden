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
    _available: bool = False

    @classmethod
    async def init(cls) -> None:
        """初始化 Redis 连接池（失败时降级）"""
        try:
            cls._pool = redis.ConnectionPool.from_url(
                settings.redis_url,
                password=settings.redis_password or None,
                max_connections=50,
                decode_responses=True,
            )
            cls._client = redis.Redis(connection_pool=cls._pool)
            # 测试连接
            await cls._client.ping()
            cls._available = True
        except Exception as e:
            print(f"[Redis] 连接失败，降级模式: {e}")
            cls._available = False
            cls._client = None

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
            raise RuntimeError("Redis is not available")
        return cls._client

    @classmethod
    def is_available(cls) -> bool:
        """Redis 是否可用"""
        return cls._available

    @classmethod
    async def ping(cls) -> bool:
        """检查 Redis 连接"""
        try:
            if cls._client:
                return await cls._client.ping()
        except Exception:
            pass
        return False


async def get_redis() -> AsyncGenerator[Redis, None]:
    """FastAPI 依赖：获取 Redis 客户端"""
    yield RedisManager.client()
