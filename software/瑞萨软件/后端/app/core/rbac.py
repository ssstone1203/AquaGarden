"""
RBAC 权限控制
"""
from enum import Enum

from app.models.user import User


class Permission(str, Enum):
    """权限枚举"""
    READ = "read"
    WRITE = "write"
    DELETE = "delete"
    ADMIN = "admin"
    DEVICE_CONTROL = "device_control"
    SYSTEM_CONFIG = "system_config"


# 角色权限映射
ROLE_PERMISSIONS: dict[str, list[Permission]] = {
    "admin": [
        Permission.READ,
        Permission.WRITE,
        Permission.DELETE,
        Permission.ADMIN,
        Permission.DEVICE_CONTROL,
        Permission.SYSTEM_CONFIG,
    ],
    "user": [
        Permission.READ,
        Permission.WRITE,
        Permission.DEVICE_CONTROL,
    ],
    "device": [
        Permission.READ,
        Permission.WRITE,
    ],
}


def has_permission(user: User, permission: Permission) -> bool:
    """检查用户是否拥有指定权限"""
    if not user.is_active:
        return False
    role_perms = ROLE_PERMISSIONS.get(user.role, [])
    return permission in role_perms


def is_admin(user: User) -> bool:
    """检查是否为管理员"""
    return user.role == "admin" and user.is_active


def can_control_device(user: User) -> bool:
    """检查是否可以控制设备"""
    return has_permission(user, Permission.DEVICE_CONTROL)
