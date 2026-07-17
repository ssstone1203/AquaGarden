import asyncio
from collections.abc import Generator
from types import SimpleNamespace
from unittest.mock import AsyncMock, patch

import pytest
from fastapi.testclient import TestClient
from pydantic import SecretStr

from app.core.config import Settings, settings
from app.core.security import get_current_user
from app.main import create_app
from app.schemas.common import AiChatMessage, SensorSnapshot
from app.services import ai_service
from app.services.hardware_serial import hardware_serial
from app.services.state import state
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


def sensor_snapshot() -> SensorSnapshot:
    return SensorSnapshot(
        water_temp=21.5,
        air_temp=24.0,
        air_humidity=68.0,
        wqi=115.0,
        soil_moisture=54.0,
    )


def test_ai_chat_uses_fresh_sensor_snapshot(client: TestClient) -> None:
    # Arrange
    snapshot = sensor_snapshot()
    llm_response = {
        "ok": True,
        "source": "llm",
        "provider": "anthropic",
        "model": "test-model",
        "analysis": "水温与浊度已纳入判断。",
        "llmOk": True,
        "llmStatus": "ok",
        "llmMessage": "大模型已成功返回内容。",
        "sensorSource": "hardware",
        "sensorSnapshot": snapshot.model_dump(),
    }

    with (
        patch.object(state, "read_fresh_or_demo", return_value=snapshot) as read_snapshot,
        patch.object(state, "has_fresh_hardware_snapshot", return_value=True) as has_fresh_snapshot,
        patch("app.api.ai.ai_service.chat", AsyncMock(return_value=llm_response)) as chat,
    ):
        # Act
        response = client.post(
            "/api/ai/chat",
            json={"message": "现在需要换水吗？", "history": []},
        )

    # Assert
    assert response.status_code == 200
    assert response.json()["sensorSource"] == "hardware"
    read_snapshot.assert_called_once_with(settings.sensors_realtime_max_age_ms)
    has_fresh_snapshot.assert_called_once_with(settings.sensors_realtime_max_age_ms)
    assert chat.await_args.args[:2] == (snapshot, True)


def test_sensor_context_describes_tds_turbidity_in_ntu() -> None:
    # Arrange / Act
    context = ai_service._sensor_context(sensor_snapshot(), from_hardware=True)

    # Assert
    assert "TDS 水质浊度 115 NTU" in context
    assert "/ 100" not in context


def test_ai_chat_sends_recent_history_and_sensor_context_to_model() -> None:
    # Arrange
    history = [
        AiChatMessage(role="user" if index % 2 == 0 else "assistant", content=f"message-{index}")
        for index in range(14)
    ]
    model_call = AsyncMock(return_value="建议先观察浊度趋势，再决定是否换水。")

    async def scenario() -> dict:
        with (
            patch.object(ai_service, "_llm_configured", return_value=True),
            patch.object(ai_service, "_call_anthropic", model_call),
            patch.object(settings, "llm_provider", "anthropic"),
        ):
            return await ai_service.chat(sensor_snapshot(), True, "需要换水吗？", history)

    # Act
    response = asyncio.run(scenario())

    # Assert
    assert response["analysis"] == "建议先观察浊度趋势，再决定是否换水。"
    assert response["sensorSource"] == "hardware"
    assert response["sensorSnapshot"]["wqi"] == 115.0
    call = model_call.await_args
    assert [item["content"] for item in call.args[2]] == [f"message-{index}" for index in range(2, 14)]
    assert "TDS 水质浊度 115 NTU" in call.args[1]


def test_ai_chat_hides_upstream_error_details() -> None:
    # Arrange
    upstream_error = "request failed at https://internal.example/v1/messages?token=secret-value"

    async def scenario() -> dict:
        with (
            patch.object(ai_service, "_llm_configured", return_value=True),
            patch.object(ai_service, "_call_anthropic", AsyncMock(side_effect=RuntimeError(upstream_error))),
            patch.object(settings, "llm_provider", "anthropic"),
        ):
            return await ai_service.chat(sensor_snapshot(), True, "当前状态怎么样？", [])

    # Act
    response = asyncio.run(scenario())

    # Assert
    assert response["source"] == "fallback"
    assert response["llmStatus"] == "error"
    assert "internal.example" not in response["llmMessage"]
    assert "secret-value" not in response["llmMessage"]


def test_llm_api_key_is_loaded_as_secret_from_internal_environment_name() -> None:
    # Arrange / Act
    configured = Settings(_env_file=None, AQUAGARDEN_LLM_API_KEY="internal-test-key")

    # Assert
    assert isinstance(configured.llm_api_key, SecretStr)
    assert configured.llm_api_key.get_secret_value() == "internal-test-key"
    assert "internal-test-key" not in repr(configured)


@pytest.mark.parametrize(
    ("auth_mode", "expected_header", "unexpected_header"),
    [
        ("bearer", "Authorization", "x-api-key"),
        ("x-api-key", "x-api-key", "Authorization"),
    ],
)
def test_anthropic_auth_mode_sends_secret_in_only_one_header(
    auth_mode: str,
    expected_header: str,
    unexpected_header: str,
) -> None:
    # Arrange
    with (
        patch.object(settings, "llm_auth_mode", auth_mode),
        patch.object(settings, "llm_api_key", SecretStr("internal-test-key")),
    ):
        # Act
        headers = ai_service._anthropic_headers()

    # Assert
    assert expected_header in headers
    assert unexpected_header not in headers
    assert list(headers.values()).count("internal-test-key") <= 1
    assert list(headers.values()).count("Bearer internal-test-key") <= 1
