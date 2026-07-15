import logging
import re
import time

from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from app.core.config import settings
from app.core.security import hash_password
from app.db.session import SessionLocal
from app.models.user import User


logger = logging.getLogger(__name__)
USERNAME_RE = re.compile(r"^[A-Za-z0-9_-]+$")
EMAIL_RE = re.compile(r"^[\w.+-]+@[\w.-]+\.[A-Za-z]{2,}$")


def ensure_admin_user(db: Session, username: str, password: str, email: str | None = None) -> bool:
    username = username.strip()
    email = email.strip() if email else None
    if not 3 <= len(username) <= 50 or not USERNAME_RE.fullmatch(username):
        raise ValueError("initial admin username must be 3..50 ASCII letters, digits, underscores, or hyphens")
    if not 8 <= len(password) <= 100:
        raise ValueError("initial admin password must be 8..100 characters")
    if email and (len(email) > 100 or not EMAIL_RE.fullmatch(email)):
        raise ValueError("initial admin email is invalid")

    existing = db.scalar(select(User).where(User.username == username))
    if existing is not None:
        if existing.role != "admin":
            logger.warning("Initial admin username already belongs to a non-admin user; no changes applied")
        return False

    admin = User(
        username=username,
        email=email,
        hashed_password=hash_password(password),
        role="admin",
        created_at=int(time.time() * 1000),
    )
    db.add(admin)
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        return False
    logger.info("Initial administrator account created for username %s", username)
    return True


def ensure_initial_admin() -> bool:
    username = settings.initial_admin_username.strip()
    password = settings.initial_admin_password.get_secret_value()
    if not username and not password:
        return False
    if not username or not password:
        raise RuntimeError("AQUAGARDEN_ADMIN_USERNAME and AQUAGARDEN_ADMIN_PASSWORD must be configured together")
    with SessionLocal() as db:
        return ensure_admin_user(db, username, password, settings.initial_admin_email)
