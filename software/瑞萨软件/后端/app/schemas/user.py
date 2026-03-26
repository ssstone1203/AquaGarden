"""
用户相关 Pydantic 模型
"""
from datetime import datetime

from pydantic import BaseModel


class UserProfileResponse(BaseModel):
    """用户资料响应"""
    id: str
    username: str
    email: str | None
    role: str
    is_active: bool
    created_at: datetime

    model_config = {"from_attributes": True}
