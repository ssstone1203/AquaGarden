"""
认证相关 Pydantic 模型
"""
from datetime import datetime

from pydantic import BaseModel, EmailStr, Field, field_validator

from app.schemas.common import TimestampMixin


# ── 注册 ──────────────────────────────────────────────────────────────────
class RegisterRequest(BaseModel):
    username: str = Field(
        ...,
        min_length=3,
        max_length=50,
        description="用户名（3-50个字符）",
    )
    email: EmailStr | None = Field(
        None,
        description="邮箱（可选）",
    )
    password: str = Field(
        ...,
        min_length=6,
        description="密码（至少6个字符）",
    )

    @field_validator("username")
    @classmethod
    def username_alphanumeric(cls, v: str) -> str:
        if not v.replace("_", "").replace("-", "").isalnum():
            raise ValueError("用户名只能包含字母、数字、下划线和连字符")
        return v


class RegisterResponse(BaseModel):
    """注册响应"""
    id: str
    username: str
    email: str | None
    created_at: datetime


# ── 登录 ──────────────────────────────────────────────────────────────────
class LoginRequest(BaseModel):
    username: str = Field(..., description="账号")
    password: str = Field(..., description="密码")


class TokenResponse(BaseModel):
    """Token 响应"""
    access_token: str
    refresh_token: str
    token_type: str = "Bearer"
    expires_in: int  # 秒


# ── 刷新 ──────────────────────────────────────────────────────────────────
class RefreshRequest(BaseModel):
    refresh_token: str


# ── 当前用户 ──────────────────────────────────────────────────────────────
class UserResponse(TimestampMixin):
    """用户响应模型"""
    id: str
    username: str
    email: str | None
    role: str
    is_active: bool
    avatar_url: str | None = None
    bio: str | None = None


# ── 密码修改 ──────────────────────────────────────────────────────────────
class ChangePasswordRequest(BaseModel):
    old_password: str = Field(..., description="旧密码")
    new_password: str = Field(..., min_length=6, description="新密码（至少6个字符）")
