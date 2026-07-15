from collections.abc import Generator
from unittest.mock import patch

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from app.core.security import hash_password
from app.db.session import Base, get_db
from app.main import create_app
from app.models.user import User
from app.services.hardware_serial import hardware_serial
from app.services.user_bootstrap import ensure_admin_user


@pytest.fixture
def db_session(tmp_path) -> Generator[Session, None, None]:
    engine = create_engine(
        f"sqlite:///{tmp_path / 'auth-test.db'}",
        connect_args={"check_same_thread": False},
    )
    Base.metadata.create_all(bind=engine)
    testing_session = sessionmaker(bind=engine, autoflush=False, autocommit=False)
    with testing_session() as session:
        yield session
    engine.dispose()


@pytest.fixture
def client(db_session: Session) -> Generator[TestClient, None, None]:
    app = create_app()

    def override_db() -> Generator[Session, None, None]:
        yield db_session

    app.dependency_overrides[get_db] = override_db
    with (
        patch("app.main.ensure_initial_admin", return_value=False),
        patch.object(hardware_serial, "start"),
        patch.object(hardware_serial, "stop"),
        TestClient(app) as test_client,
    ):
        yield test_client


def user_factory(db: Session, username: str = "member", password: str = "member123", role: str = "user") -> User:
    user = User(username=username, hashed_password=hash_password(password), role=role)
    db.add(user)
    db.commit()
    return user


def test_ensure_admin_user_creates_bcrypt_admin(db_session: Session) -> None:
    # Arrange / Act
    created = ensure_admin_user(db_session, username="admin", password="admin123")
    admin = db_session.query(User).filter(User.username == "admin").one()

    # Assert
    assert created is True
    assert admin.role == "admin"
    assert admin.hashed_password.startswith(("$2a$", "$2b$"))
    assert admin.hashed_password != "admin123"


def test_ensure_admin_user_does_not_replace_existing_password(db_session: Session) -> None:
    # Arrange
    ensure_admin_user(db_session, username="admin", password="original123")
    original_hash = db_session.query(User).filter(User.username == "admin").one().hashed_password

    # Act
    created = ensure_admin_user(db_session, username="admin", password="replacement123")
    unchanged_hash = db_session.query(User).filter(User.username == "admin").one().hashed_password

    # Assert
    assert created is False
    assert unchanged_hash == original_hash


def test_login_returns_token_and_current_user(client: TestClient, db_session: Session) -> None:
    # Arrange
    user_factory(db_session, username="admin", password="admin123", role="admin")

    # Act
    response = client.post("/api/login", json={"username": "admin", "password": "admin123"})
    token = response.json()["access_token"]
    current = client.get("/api/users/me", headers={"Authorization": f"Bearer {token}"})

    # Assert
    assert response.status_code == 200
    assert response.json()["token_type"] == "bearer"
    assert current.status_code == 200
    assert current.json()["username"] == "admin"
    assert current.json()["role"] == "admin"


def test_login_with_wrong_password_returns_generic_error(client: TestClient, db_session: Session) -> None:
    # Arrange
    user_factory(db_session, username="admin", password="admin123", role="admin")

    # Act
    response = client.post("/api/login", json={"username": "admin", "password": "wrong-password"})

    # Assert
    assert response.status_code == 401
    assert response.json() == {"detail": "用户名或密码错误"}


def test_registered_user_can_login(client: TestClient) -> None:
    # Arrange
    registration = {
        "username": "new_user",
        "email": "new_user@example.com",
        "password": "new-user-password",
    }

    # Act
    registered = client.post("/api/register", json=registration)
    logged_in = client.post(
        "/api/login",
        json={"username": registration["username"], "password": registration["password"]},
    )

    # Assert
    assert registered.status_code == 200
    assert registered.json()["user"]["username"] == "new_user"
    assert logged_in.status_code == 200
    assert logged_in.json()["token_type"] == "bearer"


def test_user_list_requires_admin_role(client: TestClient, db_session: Session) -> None:
    # Arrange
    user_factory(db_session, username="member", password="member123", role="user")
    login = client.post("/api/login", json={"username": "member", "password": "member123"})
    token = login.json()["access_token"]

    # Act
    response = client.get("/api/users", headers={"Authorization": f"Bearer {token}"})

    # Assert
    assert response.status_code == 403
    assert response.json() == {"detail": "Forbidden"}


def test_invalid_token_returns_frontend_compatible_generic_error(client: TestClient) -> None:
    # Arrange / Act
    response = client.get("/api/users/me", headers={"Authorization": "Bearer invalid-token"})

    # Assert
    assert response.status_code == 401
    assert response.json() == {"detail": "无效的认证凭据"}
