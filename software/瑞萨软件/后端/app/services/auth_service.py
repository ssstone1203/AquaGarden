"""
用户认证服务
"""
from datetime import datetime, timedelta, timezone

from jose import JWTError
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.config import settings
from app.core.security import (
    create_access_token,
    create_refresh_token,
    decode_token,
    hash_password,
    verify_password,
)
from app.models.user import User
from app.redis_client import RedisManager
from app.schemas.auth import LoginRequest, RegisterRequest


class AuthService:
    """认证业务逻辑"""

    @staticmethod
    async def register(db: AsyncSession, data: RegisterRequest) -> User:
        """用户注册"""
        # 检查用户名唯一性
        stmt = select(User).where(User.username == data.username)
        result = await db.execute(stmt)
        existing = result.scalar_one_or_none()
        if existing:
            raise ValueError("用户名已存在")

        # 检查邮箱唯一性（如果提供）
        if data.email:
            stmt = select(User).where(User.email == data.email)
            result = await db.execute(stmt)
            existing_email = result.scalar_one_or_none()
            if existing_email:
                raise ValueError("邮箱已被注册")

        # 创建用户
        user = User(
            username=data.username,
            email=data.email,
            hashed_password=hash_password(data.password),
            role="user",
        )
        db.add(user)
        await db.flush()
        await db.refresh(user)
        return user

    @staticmethod
    async def login(db: AsyncSession, data: LoginRequest) -> dict:
        """用户登录，返回 Token 信息"""
        stmt = select(User).where(User.username == data.username)
        result = await db.execute(stmt)
        user = result.scalar_one_or_none()

        if not user or not verify_password(data.password, user.hashed_password):
            raise ValueError("用户名或密码错误")

        if not user.is_active:
            raise ValueError("账号已被禁用")

        # 生成令牌
        access_token = create_access_token(subject=str(user.id))
        refresh_token = create_refresh_token(subject=str(user.id))

        # 将 Refresh Token 存入 Redis
        redis = RedisManager.client()
        await redis.setex(
            f"refresh_token:{user.id}:{refresh_token}",
            settings.refresh_token_expire_days * 86400,
            "1",
        )

        # 检查并发 Token 数量
        token_keys = []
        async for key in redis.scan_iter(f"refresh_token:{user.id}:*"):
            token_keys.append(key)

        if len(token_keys) > settings.max_concurrent_tokens:
            # 删除最早的 Token
            await redis.delete(token_keys[0])

        return {
            "access_token": access_token,
            "refresh_token": refresh_token,
            "expires_in": settings.access_token_expire_minutes * 60,
        }

    @staticmethod
    async def logout(user_id: str, refresh_token: str) -> None:
        """退出登录，将 Refresh Token 加入黑名单"""
        redis = RedisManager.client()
        await redis.delete(f"refresh_token:{user_id}:{refresh_token}")
        # 将 Access Token 加入黑名单（TTL = 剩余有效期）
        await redis.setex(f"blacklist:{user_id}", 7200, "1")

    @staticmethod
    async def refresh_access_token(db: AsyncSession, refresh_token: str) -> dict:
        """使用 Refresh Token 刷新 Access Token"""
        payload = decode_token(refresh_token)
        if payload is None:
            raise ValueError("Refresh Token 已失效")

        if payload.get("type") != "refresh":
            raise ValueError("Token 类型不正确")

        user_id = payload.get("sub")
        if not user_id:
            raise ValueError("Token 中缺少用户标识")

        # 验证 Refresh Token 是否在 Redis 中
        redis = RedisManager.client()
        exists = await redis.exists(f"refresh_token:{user_id}:{refresh_token}")
        if not exists:
            raise ValueError("Refresh Token 已失效或已被吊销")

        # 验证用户是否存在
        stmt = select(User).where(User.id == user_id)
        result = await db.execute(stmt)
        user = result.scalar_one_or_none()
        if not user or not user.is_active:
            raise ValueError("用户不存在或已被禁用")

        # 生成新的 Access Token
        new_access_token = create_access_token(subject=str(user.id))

        return {
            "access_token": new_access_token,
            "refresh_token": refresh_token,
            "expires_in": settings.access_token_expire_minutes * 60,
        }
