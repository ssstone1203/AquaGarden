import re
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.core.config import settings
from app.core.security import create_access_token, hash_password, verify_password
from app.db.session import get_db
from app.models.user import User
from app.schemas.auth import LoginRequest, RegisterRequest, TokenResponse


router = APIRouter()
EMAIL_RE = re.compile(r"^[\w.+-]+@[\w.-]+\.[a-zA-Z]{2,}$")


@router.post("/api/register")
def register(request: RegisterRequest, db: Annotated[Session, Depends(get_db)]) -> dict:
    if db.scalar(select(User).where(User.username == request.username)):
        raise HTTPException(status_code=400, detail="用户名已存在")
    email = request.email.strip() if request.email else None
    if email and not EMAIL_RE.match(email):
        raise HTTPException(status_code=400, detail="邮箱格式不正确")
    if email and db.scalar(select(User).where(User.email == email)):
        raise HTTPException(status_code=400, detail="邮箱已被注册")
    user = User(username=request.username, email=email, hashed_password=hash_password(request.password), role="user")
    db.add(user)
    db.commit()
    return {
        "success": True,
        "message": "注册成功",
        "user": {"username": user.username, "email": user.email or "", "role": user.role},
    }


@router.post("/api/login", response_model=TokenResponse)
def login(request: LoginRequest, db: Annotated[Session, Depends(get_db)]) -> TokenResponse:
    user = db.scalar(select(User).where(User.username == request.username))
    if user is None or not verify_password(request.password, user.hashed_password):
        raise HTTPException(status_code=401, detail="用户名或密码错误")
    return TokenResponse(access_token=create_access_token(user.username), expires_in=settings.jwt_expiration_minutes * 60)
