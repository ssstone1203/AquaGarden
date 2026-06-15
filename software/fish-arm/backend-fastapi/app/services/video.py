from dataclasses import dataclass
from io import BytesIO
from random import randint
from threading import Lock
from time import sleep

from PIL import Image, ImageDraw, ImageFont

from app.services.state import now_ms


BOUNDARY = "frame"


@dataclass
class DetectionBox:
    label: str
    x: float
    y: float
    width: float
    height: float
    score: float


class TankVideoState:
    def __init__(self) -> None:
        self._lock = Lock()
        self.latest_frame: bytes | None = None
        self.latest_frame_at = 0
        self.frame_seq = 0
        self.detections: list[DetectionBox] = []
        self.detections_at = 0

    def update_frame(self, frame: bytes) -> bool:
        if len(frame) < 4 or len(frame) > 1024 * 1024 or not (frame[:2] == b"\xff\xd8" and frame[-2:] == b"\xff\xd9"):
            return False
        with self._lock:
            self.latest_frame = frame
            self.latest_frame_at = now_ms()
            self.frame_seq += 1
        return True

    def set_detections(self, detections: list[DetectionBox]) -> None:
        with self._lock:
            self.detections = detections
            self.detections_at = now_ms()

    def status(self) -> dict:
        frame = self.latest_frame
        return {
            "hasFrame": frame is not None,
            "seq": self.frame_seq,
            "updatedAt": self.latest_frame_at,
            "bytes": 0 if frame is None else len(frame),
            "detectionCount": len(self.detections),
        }

    def frame_with_overlay(self) -> bytes | None:
        frame = self.latest_frame
        if frame is None:
            return None
        if not self.detections or now_ms() - self.detections_at > 5000:
            return frame
        try:
            image = Image.open(BytesIO(frame)).convert("RGB")
            draw = ImageDraw.Draw(image)
            iw, ih = image.size
            for det in self.detections:
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


tank_video = TankVideoState()
