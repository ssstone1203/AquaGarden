"""
Authentication API router for AquaGarden.
"""
from datetime import timedelta
from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session
import logging

from app.core.database import get_db
from app.core.security import hash_password, verify_password, create_access_token, verify_token
from app.models.user import User
from app.schemas.auth import RegisterRequest, LoginRequest, Token
from app.config import settings

logger = logging.getLogger(__name__)

router = APIRouter(tags=["Authentication"])


@router.post("/register", response_model=dict)
async def register(register_data: RegisterRequest, db: Session = Depends(get_db)):
    """User registration endpoint."""
    # Check if username exists
    existing_user = db.query(User).filter(User.username == register_data.username).first()
    if existing_user:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="用户名已存在"
        )
    
    # Check email if provided
    if register_data.email:
        existing_email = db.query(User).filter(User.email == register_data.email).first()
        if existing_email:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="邮箱已被注册"
            )
    
    # Hash password and create user
    hashed_password = hash_password(register_data.password)
    new_user = User(
        username=register_data.username,
        email=register_data.email,
        hashed_password=hashed_password,
        role="user"
    )
    
    db.add(new_user)
    db.commit()
    db.refresh(new_user)
    
    logger.info(f"新用户注册: {register_data.username}")
    
    return {
        "success": True,
        "message": "注册成功",
        "user": {
            "username": new_user.username,
            "email": new_user.email,
            "role": new_user.role
        }
    }


@router.post("/login", response_model=Token)
async def login(login_data: LoginRequest, db: Session = Depends(get_db)):
    """User login endpoint."""
    user = db.query(User).filter(User.username == login_data.username).first()
    
    if not user or not verify_password(login_data.password, user.hashed_password):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="用户名或密码错误",
            headers={"WWW-Authenticate": "Bearer"},
        )
    
    access_token_expires = timedelta(minutes=settings.access_token_expire_minutes)
    access_token = create_access_token(
        data={"sub": user.username}, 
        expires_delta=access_token_expires
    )
    
    return {
        "access_token": access_token, 
        "token_type": "bearer", 
        "expires_in": settings.access_token_expire_minutes * 60
    }


@router.get("/users", response_model=list)
async def get_users(db: Session = Depends(get_db), username: str = Depends(verify_token)):
    """Get all users (admin only for now)."""
    # TODO: Add role check for admin
    users = db.query(User).all()
    return [
        {
            "id": u.id, 
            "username": u.username, 
            "email": u.email, 
            "role": u.role, 
            "created_at": u.created_at.isoformat() if u.created_at else None
        } 
        for u in users
    ]
