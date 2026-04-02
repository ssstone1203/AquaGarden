"""
认证路由
POST /api/v1/auth/register
POST /api/v1/auth/login
POST /api/v1/auth/logout
POST /api/v1/auth/refresh
GET  /api/v1/auth/me
"""
from fastapi import APIRouter, Depends, HTTPException, status

from app.deps import CurrentUser, DbSession
from app.schemas.auth import (
    ChangePasswordRequest,
    LoginRequest,
    RefreshRequest,
    RegisterRequest,
    RegisterResponse,
    TokenResponse,
    UserResponse,
)
from app.services.auth_service import AuthService
from app.core.security import hash_password, verify_password

router = APIRouter()


@router.post("/register", response_model=RegisterResponse, status_code=status.HTTP_201_CREATED)
async def register(
    data: RegisterRequest,
    db: DbSession,
):
    """用户注册"""
    try:
        user = await AuthService.register(db, data)
        return RegisterResponse.model_validate(user)
    except ValueError as e:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=str(e),
        )


@router.post("/login", response_model=TokenResponse)
async def login(
    data: LoginRequest,
    db: DbSession,
):
    """用户登录，返回 JWT Access Token 和 Refresh Token"""
    try:
        tokens = await AuthService.login(db, data)
        return TokenResponse(**tokens)
    except ValueError as e:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail=str(e),
        )


@router.post("/logout")
async def logout(
    current_user: CurrentUser,
    db: DbSession,
):
    """退出登录"""
    # 注意：实际生产中前端应传递 refresh_token
    await AuthService.logout(current_user.id, "")
    return {"message": "已退出登录"}


@router.post("/refresh", response_model=TokenResponse)
async def refresh_token(
    data: RefreshRequest,
    db: DbSession,
):
    """刷新 Access Token"""
    try:
        tokens = await AuthService.refresh_access_token(db, data.refresh_token)
        return TokenResponse(**tokens)
    except ValueError as e:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail=str(e),
        )


@router.get("/me", response_model=UserResponse)
async def get_me(current_user: CurrentUser):
    """获取当前用户信息"""
    return UserResponse.model_validate(current_user)


@router.post("/password/change")
async def change_password(
    data: ChangePasswordRequest,
    current_user: CurrentUser,
    db: DbSession,
):
    """修改密码"""
    if not verify_password(data.old_password, current_user.hashed_password):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="旧密码不正确",
        )
    current_user.hashed_password = hash_password(data.new_password)
    await db.flush()
    return {"message": "密码修改成功"}
