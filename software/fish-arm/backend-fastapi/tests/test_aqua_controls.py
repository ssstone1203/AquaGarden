from collections.abc import Generator
from types import SimpleNamespace
from unittest.mock import AsyncMock, patch

import pytest
from fastapi.testclient import TestClient

from app.core.security import get_current_user
from app.main import create_app
from app.services.hardware_serial import hardware_serial
from app.services.video import usb_camera


@pytest.fixture
def client() -> Generator[TestClient, None, None]:
    app = create_app()
    app.dependency_overrides[get_current_user] = lambda: SimpleNamespace(username="operator", role="user")
    with (
        patch("app.main.init_db"),
        patch("app.main.ensure_initial_admin", return_value=False),
        patch.object(hardware_serial, "start"),
        patch.object(hardware_serial, "stop"),
        patch.object(usb_camera, "start"),
        patch.object(usb_camera, "stop"),
        TestClient(app) as test_client,
    ):
        yield test_client


def test_mechanical_arm_task_proxies_allowlisted_task_to_bridge(client: TestClient) -> None:
    # Arrange
    bridge_request = AsyncMock(return_value=(200, {"ok": True, "currentTask": "feed"}))

    with patch("app.api.aqua.bridge.bridge_request", bridge_request):
        # Act
        response = client.post("/api/aqua/tasks/feed")

    # Assert
    assert response.status_code == 200
    assert response.json() == {"ok": True, "currentTask": "feed"}
    bridge_request.assert_awaited_once_with("POST", "/api/task/feed", None)


def test_rail_position_rejects_out_of_range_value(client: TestClient) -> None:
    # Arrange
    bridge_request = AsyncMock()

    with patch("app.api.aqua.bridge.bridge_request", bridge_request):
        # Act
        response = client.post("/api/aqua/rail/position", json={"position": 5201})

    # Assert
    assert response.status_code == 400
    assert response.json() == {"detail": "position must be 0..5200"}
    bridge_request.assert_not_awaited()


def test_rail_position_proxies_valid_absolute_position(client: TestClient) -> None:
    # Arrange
    bridge_request = AsyncMock(return_value=(200, {"ok": True, "railPosition": 5200}))

    with patch("app.api.aqua.bridge.bridge_request", bridge_request):
        # Act
        response = client.post("/api/aqua/rail/position", json={"position": 5200})

    # Assert
    assert response.status_code == 200
    assert response.json()["railPosition"] == 5200
    bridge_request.assert_awaited_once_with("POST", "/api/rail/position", {"position": 5200})


def test_pump_start_uses_fastapi_serial_service_when_enabled(client: TestClient) -> None:
    # Arrange
    hardware_response = {"ok": True, "pump": {"manualOn": True, "pwm": 65}, "pwmUi": 65}

    with (
        patch.object(hardware_serial, "enabled", True),
        patch.object(hardware_serial, "pump_start", return_value=(200, hardware_response)) as pump_start,
    ):
        # Act
        response = client.post("/api/aqua/pump/start", json={"pwm": 65})

    # Assert
    assert response.status_code == 200
    assert response.json() == hardware_response
    pump_start.assert_called_once_with(65)


def test_usb_light_control_uses_confirmed_serial_service_state(client: TestClient) -> None:
    # Arrange
    hardware_response = {
        "ok": True,
        "confirmed": True,
        "mode": 19,
        "message": "usb light mode confirmed",
    }

    with patch.object(hardware_serial, "usb_light_set", return_value=(200, hardware_response)) as usb_light_set:
        # Act
        response = client.post("/api/aqua/usb-light", json={"mode": 19})

    # Assert
    assert response.status_code == 200
    assert response.json() == hardware_response
    usb_light_set.assert_called_once_with(19)


@pytest.mark.parametrize("mode", [-1, 27, True, "2"], ids=["below range", "above range", "boolean", "string"])
def test_usb_light_control_rejects_invalid_mode(client: TestClient, mode: object) -> None:
    # Arrange / Act
    response = client.post("/api/aqua/usb-light", json={"mode": mode})

    # Assert
    assert response.status_code == 422


def test_aqua_status_merges_bridge_motion_state_with_local_serial_state(client: TestClient) -> None:
    # Arrange
    local_status = {
        "ok": True,
        "connected": True,
        "busy": False,
        "currentTask": "idle",
        "phase": "fastapi-serial",
        "railPosition": None,
        "lastError": None,
        "pump": {"manualOn": True, "pwm": 70},
        "atomizer": {"state": False, "available": True, "fault": False},
        "usbLight": {"mode": 2, "ready": True, "fault": False},
    }
    remote_status = {
        "ok": True,
        "connected": True,
        "busy": True,
        "currentTask": "feed",
        "phase": "running",
        "railPosition": 900,
        "lastError": None,
        "camera": {"hasRgb": True, "hasDepth": False},
    }

    with (
        patch.object(hardware_serial, "enabled", True),
        patch.object(hardware_serial, "status", return_value=local_status),
        patch("app.api.aqua.bridge.bridge_status", AsyncMock(return_value=remote_status)),
    ):
        # Act
        response = client.get("/api/aqua/status")

    # Assert
    assert response.status_code == 200
    data = response.json()
    assert data["busy"] is True
    assert data["currentTask"] == "feed"
    assert data["railPosition"] == 900
    assert data["pump"] == {"manualOn": True, "pwm": 70}
    assert data["usbLight"] == {"mode": 2, "ready": True, "fault": False}
    assert data["bridge"]["connected"] is True
