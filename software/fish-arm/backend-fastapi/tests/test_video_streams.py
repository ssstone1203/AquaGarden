import asyncio
from collections.abc import AsyncIterator
from unittest.mock import patch

from fastapi.testclient import TestClient

from app.main import create_app
from app.services.hardware_serial import hardware_serial
from app.services.video import (
    DetectionBox,
    RaspberryPiVideoProxy,
    TankVideoState,
    UsbCameraService,
    YoloFishDetector,
    raspberry_pi_video,
    usb_camera,
)


class FakeUpstreamResponse:
    def raise_for_status(self) -> None:
        return None

    async def aiter_bytes(self) -> AsyncIterator[bytes]:
        yield b"--frame\r\nContent-Type: image/jpeg\r\n\r\njpeg\r\n"


class FakeStreamContext:
    async def __aenter__(self) -> FakeUpstreamResponse:
        return FakeUpstreamResponse()

    async def __aexit__(self, *_args) -> None:
        return None


class FakeAsyncClient:
    async def __aenter__(self):
        return self

    async def __aexit__(self, *_args) -> None:
        return None

    def stream(self, method: str, url: str) -> FakeStreamContext:
        assert method == "GET"
        assert url == "http://10.213.133.50:18080/video/rgb.mjpg"
        return FakeStreamContext()


class FakeEncodedJpeg:
    def tobytes(self) -> bytes:
        return b"\xff\xd8usb-camera-frame\xff\xd9"


class FakeCapture:
    def __init__(self, opened: bool = True) -> None:
        self.opened = opened
        self.released = False

    def isOpened(self) -> bool:
        return self.opened

    def set(self, _prop: int, _value: float) -> bool:
        return True

    def read(self):
        return True, object()

    def release(self) -> None:
        self.released = True


class FakeCv2:
    CAP_DSHOW = 700
    CAP_PROP_FRAME_WIDTH = 3
    CAP_PROP_FRAME_HEIGHT = 4
    CAP_PROP_FPS = 5
    CAP_PROP_BUFFERSIZE = 38
    CAP_PROP_FOURCC = 6
    IMWRITE_JPEG_QUALITY = 1

    def __init__(self, capture: FakeCapture) -> None:
        self.capture = capture
        self.opened_with: list[tuple[int, int | None]] = []

    def VideoCapture(self, index: int, backend: int | None = None) -> FakeCapture:
        self.opened_with.append((index, backend))
        return self.capture

    def VideoWriter_fourcc(self, *_chars: str) -> int:
        return 0

    def imencode(self, extension: str, _frame, params: list[int]):
        assert extension == ".jpg"
        assert params == [self.IMWRITE_JPEG_QUALITY, 80]
        return True, FakeEncodedJpeg()


class FakeTensor:
    def __init__(self, values) -> None:
        self.values = values

    def tolist(self):
        return self.values


class FakeYoloBoxes:
    xyxy = FakeTensor([[64.0, 48.0, 320.0, 240.0]])
    conf = FakeTensor([0.91])
    cls = FakeTensor([0.0])


class FakeYoloResult:
    boxes = FakeYoloBoxes()


class FakeYoloModel:
    names = {0: "fish"}

    def __init__(self) -> None:
        self.calls: list[dict] = []

    def predict(self, **kwargs):
        self.calls.append(kwargs)
        return [FakeYoloResult()]


class FakeFrame:
    shape = (480, 640, 3)


class FakeDetector:
    def __init__(self) -> None:
        self.calls = 0

    def detect(self, _frame):
        self.calls += 1
        return [
            DetectionBox(label="goldfish", x=0.1, y=0.1, width=0.4, height=0.4, score=0.9)
        ]

    def status(self) -> dict:
        return {"enabled": True, "loaded": True, "lastError": None}


def test_raspberry_pi_proxy_forwards_upstream_mjpeg_and_reports_connected() -> None:
    # Arrange
    proxy = RaspberryPiVideoProxy(
        source_url="http://10.213.133.50:18080/video/rgb.mjpg",
        client_factory=lambda **_kwargs: FakeAsyncClient(),
    )

    async def read_first_chunk() -> bytes:
        stream = proxy.stream()
        try:
            return await anext(stream)
        finally:
            await stream.aclose()

    # Act
    chunk = asyncio.run(read_first_chunk())

    # Assert
    assert chunk.startswith(b"--frame")
    assert proxy.status()["connected"] is True
    assert proxy.status()["configured"] is True
    assert "sourceUrl" not in proxy.status()


def test_usb_camera_capture_once_updates_tank_video_state() -> None:
    # Arrange
    capture = FakeCapture()
    video_state = TankVideoState()
    service = UsbCameraService(
        video_state=video_state,
        cv2_module=FakeCv2(capture),
        enabled=True,
        camera_index=2,
        width=640,
        height=480,
        fps=10.0,
        jpeg_quality=80,
    )

    # Act
    captured = service.capture_once()

    # Assert
    assert captured is True
    assert capture.released is True
    assert video_state.status()["hasFrame"] is True
    assert video_state.status()["source"] == "usb"
    assert service.status()["connected"] is True


def test_usb_camera_capture_once_reports_unavailable_device() -> None:
    # Arrange
    capture = FakeCapture(opened=False)
    service = UsbCameraService(
        video_state=TankVideoState(),
        cv2_module=FakeCv2(capture),
        enabled=True,
        camera_index=3,
    )

    # Act
    captured = service.capture_once()

    # Assert
    assert captured is False
    assert service.status()["connected"] is False
    assert service.status()["lastError"] == "cannot open USB camera index=3"


def test_yolo_detector_converts_model_box_to_goldfish_detection(tmp_path) -> None:
    # Arrange
    weights = tmp_path / "best.pt"
    weights.write_bytes(b"test-weights")
    model = FakeYoloModel()
    detector = YoloFishDetector(
        weights_path=weights,
        model_factory=lambda _path: model,
        enabled=True,
        confidence=0.25,
        image_size=640,
    )

    # Act
    detections = detector.detect(FakeFrame())

    # Assert
    assert detections == [
        DetectionBox(label="goldfish", x=0.1, y=0.1, width=0.4, height=0.4, score=0.91)
    ]
    assert model.calls[0]["conf"] == 0.25
    assert model.calls[0]["imgsz"] == 640
    assert model.calls[0]["verbose"] is False
    assert detector.status()["loaded"] is True


def test_usb_camera_capture_once_publishes_yolo_detections() -> None:
    # Arrange
    capture = FakeCapture()
    capture.read = lambda: (True, FakeFrame())
    video_state = TankVideoState()
    detector = FakeDetector()
    service = UsbCameraService(
        video_state=video_state,
        cv2_module=FakeCv2(capture),
        detector=detector,
        enabled=True,
        detection_every_n_frames=1,
    )

    # Act
    captured = service.capture_once()

    # Assert
    assert captured is True
    assert detector.calls == 1
    assert video_state.status()["detectionCount"] == 1
    assert service.status()["detector"]["loaded"] is True


def test_raspberry_pi_video_endpoint_returns_mjpeg_stream() -> None:
    async def one_chunk():
        yield b"--frame\r\nContent-Type: image/jpeg\r\n\r\njpeg\r\n"

    with (
        patch("app.main.ensure_initial_admin", return_value=False),
        patch.object(hardware_serial, "start"),
        patch.object(hardware_serial, "stop"),
        patch.object(usb_camera, "start"),
        patch.object(usb_camera, "stop"),
        patch.object(raspberry_pi_video, "stream", return_value=one_chunk()),
        TestClient(create_app()) as client,
    ):
        # Act
        response = client.get("/api/video/raspberry-pi")

    # Assert
    assert response.status_code == 200
    assert response.headers["content-type"] == "multipart/x-mixed-replace; boundary=frame"
    assert response.content.startswith(b"--frame")
