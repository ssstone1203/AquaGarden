from typing import Annotated

from fastapi import APIRouter, Depends
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.db.session import get_db
from app.models.user import User


router = APIRouter()


@router.get("/api/users")
def list_users(db: Annotated[Session, Depends(get_db)]) -> list[dict]:
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


def _format_ts(value) -> str:
    try:
        ts = int(value)
    except (TypeError, ValueError):
        return ""
    return str(ts)
