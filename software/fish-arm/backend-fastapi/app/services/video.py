import asyncio
import logging
import os
from dataclasses import dataclass
from io import BytesIO
from pathlib import Path
from random import randint
from threading import Lock, Thread
from time import monotonic, sleep
from typing import Any, Callable

import httpx
from PIL import Image, ImageDraw, ImageFont

from app.core.config import settings
from app.services.state import now_ms

try:
    import cv2 as _cv2
except ImportError:  # pragma: no cover - exercised only when optional runtime dependency is absent.
    _cv2 = None

BOUNDARY = "frame"
logger = logging.getLogger(__name__)


@dataclass
class DetectionBox:
    label: str
    x: float
    y: float
    width: float
    height: float
    score: float


class YoloFishDetector:
    def __init__(
        self,
        weights_path: str | Path,
        model_factory: Callable[[str], Any] | None = None,
        enabled: bool = True,
        confidence: float = 0.25,
        image_size: int = 640,
        device: str = "",
    ) -> None:
        self.weights_path = Path(weights_path)
        self.model_factory = model_factory
        self.enabled = bool(enabled)
        self.confidence = max(0.01, min(1.0, float(confidence)))
        self.image_size = max(160, int(image_size))
        self.device = device.strip()
        self._load_lock = Lock()
        self._status_lock = Lock()
        self._model: Any | None = None
        self._loaded = False
        self._last_inference_at = 0
        self._last_error: str | None = None
        self._inference_count = 0
        self._detection_count = 0

    def detect(self, frame: Any) -> list[DetectionBox]:
        if not self.enabled:
            return []
        model = self._get_model()
        if model is None:
            return []

        kwargs: dict[str, Any] = {
            "source": frame,
            "conf": self.confidence,
            "imgsz": self.image_size,
            "verbose": False,
        }
        if self.device:
            kwargs["device"] = self.device

        try:
            results = model.predict(**kwargs)
            detections = self._convert_results(results, frame)
        except Exception:
            logger.exception("YOLO fish inference failed")
            self._set_inference_status([], "YOLO inference failed")
            return []

        self._set_inference_status(detections, None)
        return detections

    def status(self) -> dict[str, Any]:
        with self._status_lock:
            return {
                "enabled": self.enabled,
                "loaded": self._loaded,
                "lastInferenceAt": self._last_inference_at,
                "lastError": self._last_error,
                "inferenceCount": self._inference_count,
                "detectionCount": self._detection_count,
            }

    def _get_model(self) -> Any | None:
        if self._model is not None:
            return self._model
        with self._load_lock:
            if self._model is not None:
                return self._model
            if not self.weights_path.is_file():
                self._set_load_error("YOLO weights file is missing")
                return None
            try:
                factory = self.model_factory
                if factory is None:
                    from ultralytics import YOLO

                    factory = YOLO
                self._model = factory(str(self.weights_path))
                with self._status_lock:
                    self._loaded = True
                    self._last_error = None
                logger.info("YOLO fish detector loaded")
            except Exception:
                logger.exception("Failed to load YOLO fish detector")
                self._set_load_error("YOLO model could not be loaded")
            return self._model

    def _convert_results(self, results: Any, frame: Any) -> list[DetectionBox]:
        frame_height, frame_width = frame.shape[:2]
        if frame_width <= 0 or frame_height <= 0:
            return []

        detections: list[DetectionBox] = []
        for result in results or []:
            boxes = getattr(result, "boxes", None)
            if boxes is None:
                continue
            xyxy_rows = _tensor_list(getattr(boxes, "xyxy", []))
            confidences = _tensor_list(getattr(boxes, "conf", []))
            class_ids = _tensor_list(getattr(boxes, "cls", []))
            names = getattr(result, "names", None) or getattr(self._model, "names", {})
            for index, coords in enumerate(xyxy_rows):
                if len(coords) < 4:
                    continue
                x1, y1, x2, y2 = (float(value) for value in coords[:4])
                x1 = max(0.0, min(float(frame_width), x1))
                y1 = max(0.0, min(float(frame_height), y1))
                x2 = max(x1, min(float(frame_width), x2))
                y2 = max(y1, min(float(frame_height), y2))
                if x2 <= x1 or y2 <= y1:
                    continue
                class_id = int(class_ids[index]) if index < len(class_ids) else 0
                model_label = _class_name(names, class_id)
                label = "goldfish" if model_label.lower() in {"fish", "goldfish"} else model_label
                score = float(confidences[index]) if index < len(confidences) else 0.0
                detections.append(
                    DetectionBox(
                        label=label,
                        x=x1 / frame_width,
                        y=y1 / frame_height,
                        width=(x2 - x1) / frame_width,
                        height=(y2 - y1) / frame_height,
                        score=max(0.0, min(1.0, score)),
                    )
                )
        return detections

    def _set_load_error(self, error: str) -> None:
        with self._status_lock:
            self._loaded = False
            self._last_error = error

    def _set_inference_status(self, detections: list[DetectionBox], error: str | None) -> None:
        with self._status_lock:
            self._last_inference_at = now_ms()
            self._last_error = error
            self._inference_count += 1
            self._detection_count = len(detections)


def _tensor_list(value: Any) -> list[Any]:
    if hasattr(value, "cpu"):
        value = value.cpu()
    if hasattr(value, "tolist"):
        value = value.tolist()
    return list(value or [])


def _class_name(names: Any, class_id: int) -> str:
    if isinstance(names, dict):
        return str(names.get(class_id, f"class-{class_id}"))
    if isinstance(names, (list, tuple)) and 0 <= class_id < len(names):
        return str(names[class_id])
    return f"class-{class_id}"


class TankVideoState:
    def __init__(self, frame_max_age_ms: int = 5000) -> None:
        self._lock = Lock()
        self.frame_max_age_ms = max(250, int(frame_max_age_ms))
        self.latest_frame: bytes | None = None
        self.latest_frame_at = 0
        self.frame_seq = 0
        self.source = "none"
        self.detections: list[DetectionBox] = []
        self.detections_at = 0

    def update_frame(self, frame: bytes, source: str = "external") -> bool:
        if len(frame) < 4 or len(frame) > 1024 * 1024 or not (frame[:2] == b"\xff\xd8" and frame[-2:] == b"\xff\xd9"):
            return False
        with self._lock:
            self.latest_frame = frame
            self.latest_frame_at = now_ms()
            self.frame_seq += 1
            self.source = source
        return True

    def set_detections(self, detections: list[DetectionBox]) -> None:
        with self._lock:
            self.detections = detections
            self.detections_at = now_ms()

    def status(self) -> dict:
        with self._lock:
            frame = self.latest_frame
            updated_at = self.latest_frame_at
            frame_seq = self.frame_seq
            source = self.source
            detection_count = len(self.detections)
        age_ms = now_ms() - updated_at if updated_at else -1
        fresh = frame is not None and 0 <= age_ms <= self.frame_max_age_ms
        return {
            "hasFrame": fresh,
            "seq": frame_seq,
            "updatedAt": updated_at,
            "bytes": 0 if frame is None else len(frame),
            "detectionCount": detection_count,
            "source": source,
            "frameAgeMs": age_ms,
        }

    def frame_with_overlay(self) -> bytes | None:
        with self._lock:
            frame = self.latest_frame
            frame_at = self.latest_frame_at
            detections = list(self.detections)
            detections_at = self.detections_at
        if frame is None or now_ms() - frame_at > self.frame_max_age_ms:
            return None
        if not detections or now_ms() - detections_at > 5000:
            return frame
        try:
            image = Image.open(BytesIO(frame)).convert("RGB")
            draw = ImageDraw.Draw(image)
            iw, ih = image.size
            for det in detections:
                x = int(det.x * iw)
                y = int(det.y * ih)
                w = max(1, int(det.width * iw))
                h = max(1, int(det.height * ih))
                draw.rectangle((x, y, x + w, y + h), outline=(0, 255, 140), width=2)
                caption = det.label if det.score <= 0 else f"{det.label} {det.score * 100 if det.score <= 1 else det.score:.0f}%"
                draw.rectangle((x, max(0, y - 20), min(iw, x + len(caption) * 8 + 8), y), fill=(0, 0, 0))
                draw.text((x + 4, max(0, y - 18)), caption, fill=(0, 255, 170))
            out = BytesIO()
            image.save(out, format="JPEG")
            return out.getvalue()
        except Exception:
            return frame


def generated_jpeg(label: str, subtitle: str, width: int = 640, height: int = 480) -> bytes:
    image = Image.new("RGB", (width, height), (randint(20, 180), randint(20, 180), randint(20, 180)))
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    draw.text((10, 20), f"Time: {now_ms()}", fill=(255, 255, 255), font=font)
    draw.text((10, 52), f"{label} - {subtitle}", fill=(0, 255, 100), font=font)
    out = BytesIO()
    image.save(out, format="JPEG")
    return out.getvalue()


def mjpeg_part(frame: bytes) -> bytes:
    return (
        f"--{BOUNDARY}\r\nContent-Type: image/jpeg\r\nContent-Length: {len(frame)}\r\n\r\n".encode()
        + frame
        + b"\r\n"
    )


def generated_stream(label: str, subtitle: str, delay: float = 0.1):
    while True:
        yield mjpeg_part(generated_jpeg(label, subtitle))
        sleep(delay)


class RaspberryPiVideoProxy:
    def __init__(
        self,
        source_url: str,
        client_factory: Callable[..., Any] = httpx.AsyncClient,
        connect_timeout_seconds: float = 3.0,
        reconnect_delay_seconds: float = 2.0,
    ) -> None:
        self.source_url = source_url.strip()
        self.client_factory = client_factory
        self.connect_timeout_seconds = max(0.2, float(connect_timeout_seconds))
        self.reconnect_delay_seconds = max(0.1, float(reconnect_delay_seconds))
        self._lock = Lock()
        self._connected = False
        self._last_chunk_at = 0
        self._last_error: str | None = None

    async def stream(self):
        if not self.source_url:
            while True:
                self._set_status(False, "Raspberry Pi camera is not configured")
                yield mjpeg_part(generated_jpeg("Raspberry Pi", "Camera source not configured"))
                await asyncio.sleep(1.0)

        timeout = httpx.Timeout(None, connect=self.connect_timeout_seconds)
        while True:
            try:
                async with self.client_factory(
                    timeout=timeout,
                    follow_redirects=False,
                    trust_env=False,
                ) as client:
                    async with client.stream("GET", self.source_url) as response:
                        response.raise_for_status()
                        self._set_status(True, None)
                        async for chunk in response.aiter_bytes():
                            if not chunk:
                                continue
                            self._set_status(True, None, chunk_received=True)
                            yield chunk
                self._set_status(False, "Raspberry Pi camera stream ended")
            except asyncio.CancelledError:
                raise
            except Exception as exc:
                logger.warning("Raspberry Pi camera proxy connection failed: %s", exc)
                self._set_status(False, "Raspberry Pi camera is unavailable")

            yield mjpeg_part(generated_jpeg("Raspberry Pi", "Camera unavailable"))
            await asyncio.sleep(self.reconnect_delay_seconds)

    def status(self) -> dict[str, Any]:
        with self._lock:
            return {
                "configured": bool(self.source_url),
                "connected": self._connected,
                "lastChunkAt": self._last_chunk_at,
                "lastError": self._last_error,
            }

    def _set_status(self, connected: bool, error: str | None, chunk_received: bool = False) -> None:
        with self._lock:
            self._connected = connected
            self._last_error = error
            if chunk_received:
                self._last_chunk_at = now_ms()


class UsbCameraService:
    def __init__(
        self,
        video_state: TankVideoState,
        cv2_module: Any = _cv2,
        detector: Any | None = None,
        enabled: bool = True,
        camera_index: int = 0,
        width: int = 640,
        height: int = 480,
        fps: float = 10.0,
        jpeg_quality: int = 80,
        reconnect_delay_seconds: float = 3.0,
        detection_every_n_frames: int = 3,
    ) -> None:
        self.video_state = video_state
        self.cv2 = cv2_module
        self.detector = detector
        self.enabled = bool(enabled)
        self.camera_index = int(camera_index)
        self.width = max(160, int(width))
        self.height = max(120, int(height))
        self.fps = max(0.5, float(fps))
        self.jpeg_quality = max(30, min(95, int(jpeg_quality)))
        self.reconnect_delay_seconds = max(0.1, float(reconnect_delay_seconds))
        self.detection_every_n_frames = max(1, int(detection_every_n_frames))
        self._lock = Lock()
        self._running = False
        self._connected = False
        self._last_frame_at = 0
        self._last_error: str | None = None
        self._thread: Thread | None = None
        self._capture = None
        self._frame_count = 0

    def start(self) -> None:
        if not self.enabled or self._running:
            return
        if self.cv2 is None:
            self._set_status(False, "OpenCV is not installed")
            return
        self._running = True
        self._thread = Thread(target=self._run, name="aquagarden-usb-camera", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._running = False
        thread = self._thread
        if thread is not None and thread.is_alive():
            thread.join(timeout=2.0)
        capture = self._capture
        if thread is not None and thread.is_alive() and capture is not None:
            try:
                capture.release()
            except Exception:
                pass
        self._thread = None
        self._capture = None
        self._set_status(False, self._last_error)

    def capture_once(self) -> bool:
        capture = self._open_capture()
        if capture is None:
            return False
        try:
            self._configure_capture(capture)
            return self._capture_frame(capture)
        finally:
            capture.release()

    def status(self) -> dict[str, Any]:
        with self._lock:
            status = {
                "enabled": self.enabled,
                "connected": self._connected,
                "cameraIndex": self.camera_index,
                "lastFrameAt": self._last_frame_at,
                "lastError": self._last_error,
            }
        status["detector"] = (
            self.detector.status()
            if self.detector is not None
            else {"enabled": False, "loaded": False, "lastError": None}
        )
        return status

    def _run(self) -> None:
        interval = 1.0 / self.fps
        while self._running:
            capture = self._open_capture()
            if capture is None:
                sleep(self.reconnect_delay_seconds)
                continue
            self._capture = capture
            self._configure_capture(capture)
            try:
                while self._running:
                    started = monotonic()
                    if not self._capture_frame(capture):
                        break
                    sleep(max(0.001, interval - (monotonic() - started)))
            finally:
                capture.release()
                self._capture = None
                self._set_status(False, self._last_error)
            if self._running:
                sleep(self.reconnect_delay_seconds)

    def _open_capture(self):
        if not self.enabled:
            self._set_status(False, "USB camera capture is disabled")
            return None
        if self.cv2 is None:
            self._set_status(False, "OpenCV is not installed")
            return None
        capture = None
        try:
            if os.name == "nt" and hasattr(self.cv2, "CAP_DSHOW"):
                capture = self.cv2.VideoCapture(self.camera_index, self.cv2.CAP_DSHOW)
                if capture is not None and not capture.isOpened():
                    capture.release()
                    capture = self.cv2.VideoCapture(self.camera_index)
            else:
                capture = self.cv2.VideoCapture(self.camera_index)
            if capture is not None and capture.isOpened():
                return capture
            if capture is not None:
                capture.release()
            self._set_status(False, f"cannot open USB camera index={self.camera_index}")
            return None
        except Exception:
            logger.exception("Failed to open USB camera")
            if capture is not None:
                try:
                    capture.release()
                except Exception:
                    pass
            self._set_status(False, f"cannot open USB camera index={self.camera_index}")
            return None

    def _configure_capture(self, capture) -> None:
        for prop, value in (
            (self.cv2.CAP_PROP_FRAME_WIDTH, self.width),
            (self.cv2.CAP_PROP_FRAME_HEIGHT, self.height),
            (self.cv2.CAP_PROP_FPS, self.fps),
            (self.cv2.CAP_PROP_BUFFERSIZE, 1),
        ):
            try:
                capture.set(prop, value)
            except Exception:
                pass
        try:
            capture.set(self.cv2.CAP_PROP_FOURCC, self.cv2.VideoWriter_fourcc(*"MJPG"))
        except Exception:
            pass

    def _capture_frame(self, capture) -> bool:
        try:
            ok, frame = capture.read()
            if not ok or frame is None:
                self._set_status(False, "USB camera read failed")
                return False
            self._frame_count += 1
            if self.detector is not None and self._frame_count % self.detection_every_n_frames == 0:
                try:
                    self.video_state.set_detections(self.detector.detect(frame))
                except Exception:
                    logger.exception("USB camera detector failed")
                    self.video_state.set_detections([])
            encoded, buffer = self.cv2.imencode(
                ".jpg",
                frame,
                [int(self.cv2.IMWRITE_JPEG_QUALITY), self.jpeg_quality],
            )
            if not encoded:
                self._set_status(False, "USB camera JPEG encode failed")
                return False
            if not self.video_state.update_frame(buffer.tobytes(), source="usb"):
                self._set_status(False, "USB camera produced an invalid JPEG frame")
                return False
            self._set_status(True, None, frame_received=True)
            return True
        except Exception:
            logger.exception("USB camera frame capture failed")
            self._set_status(False, "USB camera capture failed")
            return False

    def _set_status(self, connected: bool, error: str | None, frame_received: bool = False) -> None:
        with self._lock:
            self._connected = connected
            self._last_error = error
            if frame_received:
                self._last_frame_at = now_ms()


tank_video = TankVideoState(frame_max_age_ms=settings.tank_video_frame_max_age_ms)
raspberry_pi_video = RaspberryPiVideoProxy(
    source_url=settings.raspberry_pi_camera_url,
    connect_timeout_seconds=settings.raspberry_pi_camera_connect_timeout_seconds,
    reconnect_delay_seconds=settings.raspberry_pi_camera_reconnect_delay_seconds,
)
yolo_fish_detector = YoloFishDetector(
    weights_path=(
        Path(settings.tank_yolo_weights_path)
        if Path(settings.tank_yolo_weights_path).is_absolute()
        else Path(__file__).resolve().parents[5] / settings.tank_yolo_weights_path
    ),
    enabled=settings.tank_yolo_enabled,
    confidence=settings.tank_yolo_confidence,
    image_size=settings.tank_yolo_image_size,
    device=settings.tank_yolo_device,
)
usb_camera = UsbCameraService(
    video_state=tank_video,
    detector=yolo_fish_detector,
    enabled=settings.tank_usb_camera_enabled,
    camera_index=settings.tank_usb_camera_index,
    width=settings.tank_usb_camera_width,
    height=settings.tank_usb_camera_height,
    fps=settings.tank_usb_camera_fps,
    jpeg_quality=settings.tank_usb_camera_jpeg_quality,
    reconnect_delay_seconds=settings.tank_usb_camera_reconnect_delay_seconds,
    detection_every_n_frames=settings.tank_yolo_every_n_frames,
)
