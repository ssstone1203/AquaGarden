from typing import Annotated

from fastapi import APIRouter, Depends
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.core.security import get_current_user, require_admin
from app.db.session import get_db
from app.models.user import User


router = APIRouter()


@router.get("/api/users")
def list_users(
    db: Annotated[Session, Depends(get_db)],
    _: Annotated[User, Depends(require_admin)],
) -> list[dict]:
    rows = db.scalars(select(User).order_by(User.id.asc())).all()
    return [
        {
            "id": u.id,
            "username": u.username,
            "email": u.email or "",
            "role": u.role,
            "created_at": _format_ts(u.created_at),
        }
        for u in rows
    ]


@router.get("/api/users/me")
def current_user(user: Annotated[User, Depends(get_current_user)]) -> dict:
    return _user_data(user)


def _user_data(user: User) -> dict:
    return {
        "id": user.id,
        "username": user.username,
        "email": user.email or "",
        "role": user.role,
        "created_at": _format_ts(user.created_at),
    }


def _format_ts(value) -> str:
    try:
        ts = int(value)
    except (TypeError, ValueError):
        return ""
    return str(ts)
