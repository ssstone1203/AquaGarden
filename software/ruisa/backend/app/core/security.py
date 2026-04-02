"""
JWT 令牌与密码哈希安全工具
"""
from datetime import datetime, timedelta, timezone
from typing import Any, Dict, Optional

import bcrypt
from jose import jwt

from app.config import settings

_BCRYPT_ROUNDS = 12

# ── 密码哈希（直接使用 bcrypt，避免 passlib 与 bcrypt>=4.1 的兼容性故障）────────


def hash_password(plain_password: str) -> str:
    """对密码进行 bcrypt 哈希"""
    pw = plain_password.encode("utf-8")
    if len(pw) > 72:
        pw = pw[:72]
    digest = bcrypt.hashpw(pw, bcrypt.gensalt(rounds=_BCRYPT_ROUNDS))
    return digest.decode("ascii")


def verify_password(plain_password: str, hashed_password: str) -> bool:
    """校验密码是否匹配"""
    try:
        pw = plain_password.encode("utf-8")
        if len(pw) > 72:
            pw = pw[:72]
        return bcrypt.checkpw(pw, hashed_password.encode("ascii"))
    except ValueError:
        return False


# ── JWT Token ────────────────────────────────────────────────────────────────
def create_access_token(
    subject: str,
    expires_delta: Optional[timedelta] = None,
    extra_claims: Optional[Dict[str, Any]] = None,
) -> str:
    """创建 Access Token"""
    if expires_delta:
        expire = datetime.now(timezone.utc) + expires_delta
    else:
        expire = datetime.now(timezone.utc) + timedelta(
            minutes=settings.access_token_expire_minutes
        )

    to_encode: Dict[str, Any] = {
        "exp": expire,
        "sub": str(subject),
        "type": "access",
    }
    if extra_claims:
        to_encode.update(extra_claims)

    encoded_jwt = jwt.encode(
        to_encode,
        settings.jwt_secret_key,
        algorithm=settings.jwt_algorithm,
    )
    return encoded_jwt


def create_refresh_token(
    subject: str,
    expires_delta: Optional[timedelta] = None,
) -> str:
    """创建 Refresh Token"""
    if expires_delta:
        expire = datetime.now(timezone.utc) + expires_delta
    else:
        expire = datetime.now(timezone.utc) + timedelta(
            days=settings.refresh_token_expire_days
        )

    to_encode: Dict[str, Any] = {
        "exp": expire,
        "sub": str(subject),
        "type": "refresh",
    }

    encoded_jwt = jwt.encode(
        to_encode,
        settings.jwt_secret_key,
        algorithm=settings.jwt_algorithm,
    )
    return encoded_jwt


def decode_token(token: str) -> Optional[Dict[str, Any]]:
    """
    解码并验证 JWT Token。
    返回 payload dict，验证失败返回 None。
    """
    try:
        payload = jwt.decode(
            token,
            settings.jwt_secret_key,
            algorithms=[settings.jwt_algorithm],
        )
        return payload
    except jwt.ExpiredSignatureError:
        return None
    except jwt.JWTClaimsError:
        return None
    except Exception:
        return None
