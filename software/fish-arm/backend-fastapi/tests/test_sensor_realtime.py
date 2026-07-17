import asyncio
import threading
from unittest.mock import Mock, patch

import pytest
from fastapi.testclient import TestClient
from sqlalchemy.exc import OperationalError

from app.main import create_app
from app.schemas.common import SensorSnapshot
from app.services.hardware_serial import (
    AquaTelemetry,
    HardwareSerialService,
    decode_uplink_frame,
    hardware_serial,
    pack_downlink_frame,
    pump_command_frames,
)
from app.services.logs import WebSocketHub, hub, sensor_message
from app.services.state import now_ms, state
from app.services.video import usb_camera


def snapshot_factory(**overrides: float) -> SensorSnapshot:
    values = {
        "water_temp": 23.4,
        "air_temp": 25.6,
        "air_humidity": 61.2,
        "wqi": 82.0,
        "soil_moisture": 47.0,
    }
    values.update(overrides)
    return SensorSnapshot(**values)


def test_sensor_message_matches_frontend_contract() -> None:
    # Arrange
    snapshot = snapshot_factory()

    # Act
    message = sensor_message(snapshot, timestamp_ms=123456, source="hardware")

    # Assert
    assert message == {
        "type": "sensor_data",
        "source": "hardware",
        "ts": 123456,
        "water_temp": 23.4,
        "air_temp": 25.6,
        "air_humidity": 61.2,
        "wqi": 82.0,
        "soil_moisture": 47.0,
        "data": {
            "water_temp": 23.4,
            "air_temp": 25.6,
            "air_humidity": 61.2,
            "wqi": 82.0,
            "soil_moisture": 47.0,
        },
    }


def test_websocket_connection_immediately_receives_current_snapshot() -> None:
    # Arrange
    original_snapshot = state.latest_real
    original_timestamp = state.latest_real_ts
    original_details = state.read_sensor_details()
    timestamp_ms = now_ms()
    state.update_sensor(
        snapshot_factory(water_temp=28.5),
        timestamp_ms=timestamp_ms,
        details={"tds_ntu": 320, "pump_pwm": 60, "alarm_flags": 0},
    )

    try:
        with (
            patch("app.main.ensure_initial_admin", return_value=False),
            patch.object(hardware_serial, "start"),
            patch.object(hardware_serial, "stop"),
            patch.object(usb_camera, "start"),
            patch.object(usb_camera, "stop"),
            TestClient(create_app()) as client,
            client.websocket_connect("/ws/logs") as websocket,
        ):
            # Act
            message = websocket.receive_json()

            # Assert
            assert message["type"] == "sensor_data"
            assert message["source"] == "hardware"
            assert message["water_temp"] == 28.5
            assert message["ts"] == timestamp_ms
            assert message["tds_ntu"] == 320
            assert message["pump_pwm"] == 60
            assert message["data"]["tds_ntu"] == 320
    finally:
        state.latest_real = original_snapshot
        state.latest_real_ts = original_timestamp
        state.latest_sensor_details = original_details


def test_websocket_hub_schedules_thread_broadcast_on_bound_loop() -> None:
    class FakeWebSocket:
        def __init__(self) -> None:
            self.messages: list[dict] = []

        async def accept(self) -> None:
            return None

        async def send_json(self, payload: dict) -> None:
            self.messages.append(payload)

    async def scenario() -> None:
        # Arrange
        hub = WebSocketHub()
        websocket = FakeWebSocket()
        await hub.connect(websocket)  # type: ignore[arg-type]
        hub.bind_loop(asyncio.get_running_loop())
        payload = {"type": "sensor_data", "water_temp": 24.0}
        published: list[bool] = []

        # Act
        thread = threading.Thread(target=lambda: published.append(hub.broadcast_from_thread(payload)))
        thread.start()
        thread.join()
        await asyncio.sleep(0)
        await asyncio.sleep(0)

        # Assert
        assert published == [True]
        assert websocket.messages == [payload]

    asyncio.run(scenario())


def test_serial_snapshot_persistence_failure_keeps_realtime_path_alive() -> None:
    # Arrange
    original_snapshot = state.latest_real
    original_timestamp = state.latest_real_ts
    service = HardwareSerialService()
    snapshot = snapshot_factory(water_temp=19.8)
    db = Mock()
    database_error = OperationalError("INSERT", {}, Exception("readonly"))

    try:
        with (
            patch("app.services.hardware_serial.SessionLocal", return_value=db),
            patch("app.services.hardware_serial._save_reading", side_effect=database_error),
            patch.object(hub, "broadcast_from_thread", return_value=True) as broadcast,
        ):
            # Act
            service._publish_snapshot(snapshot)

        # Assert
        assert state.read_sensors().water_temp == 19.8
        broadcast.assert_called_once()
        assert service.serial_status()["lastPersistError"] == "sensor history persistence failed"
        db.close.assert_called_once()
    finally:
        state.latest_real = original_snapshot
        state.latest_real_ts = original_timestamp


def test_decode_uplink_frame_uses_authoritative_v2_layout() -> None:
    # Arrange
    frame = bytes.fromhex(
        "55 AA 02 85 1E 00 F5 DD 7D 00 BB 00 0B 03 B0 00 64 00 00 "
        "3C 01 00 FF 00 00 00 44 00 00 00 A0 1F 00 00 00 00 8A 95"
    )

    # Act
    telemetry = decode_uplink_frame(frame)

    # Assert
    assert telemetry.sequence == 0x85
    assert telemetry.timestamp_ms == 0x007DDDF5
    assert telemetry.air_temp == 18.7
    assert telemetry.air_humidity == 77.9
    assert telemetry.water_temp == 17.6
    assert telemetry.soil_moisture == 100
    assert telemetry.tds_ntu == 0
    assert telemetry.pump_pwm == 60
    assert telemetry.need_watering is True
    assert telemetry.atomizer_state == 0
    assert telemetry.usb_light_mode == 0xFF
    assert telemetry.alarm_flags == 0x44
    assert telemetry.alarms == ["air_read_fail", "tds_low"]
    assert telemetry.air_retry_count == 8096
    assert telemetry.tds_retry_count == 0
    assert telemetry.uwt_retry_count == 0


def test_decode_uplink_frame_rejects_deprecated_v1_layout() -> None:
    # Arrange
    frame = bytearray.fromhex(
        "55 AA 02 85 1E 00 F5 DD 7D 00 BB 00 0B 03 B0 00 64 00 00 "
        "3C 01 00 FF 00 00 00 44 00 00 00 A0 1F 00 00 00 00 8A 95"
    )
    frame[2] = 0x01

    # Act / Assert
    with pytest.raises(ValueError, match="version"):
        decode_uplink_frame(bytes(frame))


@pytest.mark.parametrize(
    ("action", "power", "enable", "expected_hex"),
    [
        ("START", 80, 1, "5A A5 02 04 50 FD FD"),
        ("STOP", 0, 1, "5A A5 01 05 C3 4C"),
        ("PWM", 50, 1, "5A A5 02 06 32 7D 74"),
        ("AUTO", 0, 0, "5A A5 01 07 42 8D"),
        ("MANUAL", 75, 1, "5A A5 03 01 01 4B 50 DB"),
    ],
    ids=["start", "stop", "pwm", "auto", "manual"],
)
def test_pump_command_frames_match_firmware_command_table(
    action: str,
    power: int,
    enable: int,
    expected_hex: str,
) -> None:
    # Arrange
    expected = bytes.fromhex(expected_hex)

    # Act
    frames = list(pump_command_frames(action, power, enable))

    # Assert
    assert frames == [expected]


@pytest.mark.parametrize(
    ("state_value", "expected_hex"),
    [
        (0, "5A A5 02 09 00 F9 51"),
        (1, "5A A5 02 09 01 38 91"),
    ],
    ids=["off", "on"],
)
def test_atomizer_downlink_matches_firmware_command_table(state_value: int, expected_hex: str) -> None:
    # Arrange / Act
    frame = pack_downlink_frame(0x09, bytes([state_value]))

    # Assert
    assert frame == bytes.fromhex(expected_hex)


def test_atomizer_set_confirms_state_from_subsequent_telemetry() -> None:
    class RecordingSerial:
        def __init__(self) -> None:
            self.is_open = True
            self.frames: list[bytes] = []
            self.written = threading.Event()

        def write(self, frame: bytes) -> int:
            self.frames.append(frame)
            self.written.set()
            return len(frame)

        def flush(self) -> None:
            return None

    def telemetry(atomizer_state: int, sequence: int) -> AquaTelemetry:
        return AquaTelemetry(
            sequence=sequence,
            timestamp_ms=sequence * 250,
            air_temp=20.0,
            air_humidity=60.0,
            water_temp=18.0,
            soil_moisture=50,
            tds_ntu=300,
            pump_pwm=0,
            need_watering=False,
            atomizer_state=atomizer_state,
            usb_light_mode=0xFF,
            alarm_flags=0,
            air_retry_count=0,
            tds_retry_count=0,
            uwt_retry_count=0,
        )

    # Arrange
    service = HardwareSerialService()
    serial_port = RecordingSerial()
    service._serial = serial_port
    result: list[tuple[int, dict]] = []

    with patch.object(service, "_publish_snapshot"):
        service._handle_telemetry(telemetry(atomizer_state=0, sequence=1))
        worker = threading.Thread(
            target=lambda: result.append(service.atomizer_set(True, confirmation_timeout_ms=500)),
        )

        # Act
        worker.start()
        assert serial_port.written.wait(timeout=0.5)
        service._handle_telemetry(telemetry(atomizer_state=1, sequence=2))
        worker.join(timeout=1)

    # Assert
    assert not worker.is_alive()
    assert serial_port.frames == [bytes.fromhex("5A A5 02 09 01 38 91")]
    assert result == [
        (
            200,
            {
                "ok": True,
                "confirmed": True,
                "state": True,
                "message": "atomizer state confirmed",
            },
        )
    ]


def test_atomizer_set_without_followup_telemetry_returns_gateway_timeout() -> None:
    class RecordingSerial:
        is_open = True

        def __init__(self) -> None:
            self.frames: list[bytes] = []

        def write(self, frame: bytes) -> int:
            self.frames.append(frame)
            return len(frame)

        def flush(self) -> None:
            return None

    # Arrange
    service = HardwareSerialService()
    serial_port = RecordingSerial()
    service._serial = serial_port

    # Act
    status_code, response = service.atomizer_set(True, confirmation_timeout_ms=0)

    # Assert
    assert serial_port.frames == [bytes.fromhex("5A A5 02 09 01 38 91")]
    assert status_code == 504
    assert response == {
        "ok": False,
        "confirmed": False,
        "state": None,
        "message": "atomizer state confirmation timed out",
    }


@pytest.mark.parametrize(
    ("mode", "expected_hex"),
    [
        (0, "5A A5 02 08 00 F8 C1"),
        (19, "5A A5 02 08 13 B9 0C"),
        (26, "5A A5 02 08 1A 79 0A"),
    ],
    ids=["red steady", "red fast with horn", "off"],
)
def test_usb_light_downlink_matches_firmware_command_table(mode: int, expected_hex: str) -> None:
    # Arrange / Act
    frame = pack_downlink_frame(0x08, bytes([mode]))

    # Assert
    assert frame == bytes.fromhex(expected_hex)


def test_usb_light_set_confirms_mode_from_subsequent_telemetry() -> None:
    class RecordingSerial:
        def __init__(self) -> None:
            self.is_open = True
            self.frames: list[bytes] = []
            self.written = threading.Event()

        def write(self, frame: bytes) -> int:
            self.frames.append(frame)
            self.written.set()
            return len(frame)

        def flush(self) -> None:
            return None

    def telemetry(usb_light_mode: int, sequence: int) -> AquaTelemetry:
        return AquaTelemetry(
            sequence=sequence,
            timestamp_ms=sequence * 250,
            air_temp=20.0,
            air_humidity=60.0,
            water_temp=18.0,
            soil_moisture=50,
            tds_ntu=300,
            pump_pwm=0,
            need_watering=False,
            atomizer_state=0,
            usb_light_mode=usb_light_mode,
            alarm_flags=0,
            air_retry_count=0,
            tds_retry_count=0,
            uwt_retry_count=0,
        )

    # Arrange
    service = HardwareSerialService()
    serial_port = RecordingSerial()
    service._serial = serial_port
    result: list[tuple[int, dict]] = []

    with patch.object(service, "_publish_snapshot"):
        service._handle_telemetry(telemetry(usb_light_mode=0xFF, sequence=1))
        worker = threading.Thread(
            target=lambda: result.append(service.usb_light_set(19, confirmation_timeout_ms=500)),
        )

        # Act
        worker.start()
        assert serial_port.written.wait(timeout=0.5)
        service._handle_telemetry(telemetry(usb_light_mode=19, sequence=2))
        worker.join(timeout=1)

    # Assert
    assert not worker.is_alive()
    assert serial_port.frames == [bytes.fromhex("5A A5 02 08 13 B9 0C")]
    assert result == [
        (
            200,
            {
                "ok": True,
                "confirmed": True,
                "mode": 19,
                "message": "usb light mode confirmed",
            },
        )
    ]
