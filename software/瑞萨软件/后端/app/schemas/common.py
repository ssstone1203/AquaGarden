"""
通用响应与分页模型
"""
from datetime import datetime
from typing import Any, Generic, Optional, TypeVar

from pydantic import BaseModel, ConfigDict

T = TypeVar("T")


class ErrorDetail(BaseModel):
    """统一错误详情"""
    code: str
    message: str
    details: Optional[dict[str, Any]] = None
    request_id: Optional[str] = None


class ErrorResponse(BaseModel):
    """统一错误响应"""
    error: ErrorDetail


class BaseResponse(BaseModel, Generic[T]):
    """通用成功响应"""
    model_config = ConfigDict(from_attributes=True)

    data: Optional[T] = None
    message: Optional[str] = None


class PaginatedData(BaseModel, Generic[T]):
    """分页数据"""
    total: int
    page: int
    page_size: int
    total_pages: int
    data: list[T]


class PageParams(BaseModel):
    """分页参数"""
    page: int = 1
    page_size: int = 20

    @property
    def offset(self) -> int:
        return (self.page - 1) * self.page_size

    @property
    def limit(self) -> int:
        return self.page_size


class SensorStatusValues(BaseModel):
    """传感器状态值"""
    value: Optional[float | int] = None
    unit: Optional[str] = None
    status: str = "normal"
    threshold_min: Optional[float] = None
    threshold_max: Optional[float] = None
    updated_at: Optional[datetime] = None


class TimestampMixin(BaseModel):
    """时间戳混入"""
    created_at: Optional[datetime] = None
    updated_at: Optional[datetime] = None
