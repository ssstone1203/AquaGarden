"""
serial_bridge.py - Tank USB camera + YOLO bridge
------------------------------------------------

This script no longer opens MCU serial ports. Spring Boot owns COM3 for
sensors and pump control. This process only:
  - reads the tank USB camera with OpenCV
  - POSTs JPEG frames to /api/video/tank/ingest
  - optionally runs YOLO and POSTs boxes to /api/video/tank/detections

Dependencies:
    pip install requests opencv-python ultralytics

Examples:
    python serial_bridge.py --backend http://localhost:8090 --tank-camera-index 0
    python serial_bridge.py --backend http://localhost:8090 --tank-camera-index 1 --no-yolo
    python serial_bridge.py --backend http://localhost:8090 --yolo-weights E:/path/best.pt --yolo-device cpu
"""

from __future__ import annotations

import argparse
import logging
import threading
import time
from pathlib import Path

import requests
from requests.adapters import HTTPAdapter

_REPO_ROOT = Path(__file__).resolve().parents[2]
_DEFAULT_YOLO_WEIGHTS = _REPO_ROOT / "model" / "yolo_fish" / "runs" / "yolo11n_fish_new" / "weights" / "best.pt"

DEFAULT_TANK_CAMERA_INDEX = 0
DEFAULT_BACKEND = "http://127.0.0.1:8090"
DEFAULT_CAMERA_FPS = 6.0
DEFAULT_YOLO_EVERY_N = 5
JPEG_ENCODE_QUALITY = 80

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("tank-camera")

_yolo_predict_lock = threading.Lock()


def create_http_session() -> requests.Session:
    session = requests.Session()
    adapter = HTTPAdapter(pool_connections=1, pool_maxsize=2, max_retries=0)
    session.mount("http://", adapter)
    session.mount("https://", adapter)
    session.headers.update({"Connection": "keep-alive"})
    return session


def wait_for_backend(backend: str, timeout: float = 3.0) -> None:
    url = backend.rstrip("/") + "/api/video/tank/status"
    while True:
        try:
            r = requests.get(url, timeout=timeout)
            if r.ok:
                log.info("Backend reachable: %s", backend)
                return
            log.warning("Backend responded HTTP %s; retrying in 2s", r.status_code)
        except requests.RequestException as e:
            log.warning("Backend not reachable (%s); retrying in 2s", e)
        time.sleep(2.0)


def post_tank_frame(session: requests.Session, backend: str, frame: bytes, timeout: float = 5.0) -> tuple[bool, str]:
    url = backend.rstrip("/") + "/api/video/tank/ingest"
    try:
        r = session.post(url, data=frame, headers={"Content-Type": "image/jpeg"}, timeout=timeout)
        if r.ok:
            return True, f"HTTP {r.status_code}"
        detail = r.text.strip()
        if len(detail) > 200:
            detail = detail[:200] + "..."
        return False, f"HTTP {r.status_code} {detail or '(empty body)'}"
    except requests.RequestException as e:
        return False, str(e)


def post_tank_detections(session: requests.Session, backend: str, detections: list[dict], timeout: float = 2.0) -> tuple[bool, str]:
    url = backend.rstrip("/") + "/api/video/tank/detections"
    try:
        r = session.post(url, json={"detections": detections}, timeout=timeout)
        if r.ok:
            return True, f"HTTP {r.status_code}"
        detail = r.text.strip()
        if len(detail) > 200:
            detail = detail[:200] + "..."
        return False, f"HTTP {r.status_code} {detail or '(empty body)'}"
    except requests.RequestException as e:
        return False, str(e)


def run_yolo_on_bgr_frame(model, frame, conf: float, device: str | None) -> list[dict]:
    h, w = frame.shape[:2]
    if w <= 0 or h <= 0:
        return []

    kwargs: dict = {"conf": conf, "verbose": False, "imgsz": 640}
    if device:
        kwargs["device"] = device

    results = model.predict(frame, **kwargs)
    detections: list[dict] = []
    for result in results:
        if result.boxes is None or len(result.boxes) == 0:
            continue
        names = getattr(result, "names", None) or {}
        for box in result.boxes:
            x1, y1, x2, y2 = box.xyxy[0].tolist()
            cls_id = int(box.cls[0])
            score = float(box.conf[0])
            label = names.get(cls_id, str(cls_id)) if isinstance(names, dict) else str(cls_id)
            detections.append(
                {
                    "label": label,
                    "x": max(0.0, min(1.0, x1 / w)),
                    "y": max(0.0, min(1.0, y1 / h)),
                    "width": max(0.0, min(1.0, (x2 - x1) / w)),
                    "height": max(0.0, min(1.0, (y2 - y1) / h)),
                    "score": score,
                }
            )
    return detections


def maybe_infer_tank_and_post(
    session: requests.Session,
    backend: str,
    yolo_model,
    frame_bgr,
    conf: float,
    device: str | None,
    verbose: bool,
) -> None:
    if yolo_model is None or frame_bgr is None:
        return
    try:
        with _yolo_predict_lock:
            detections = run_yolo_on_bgr_frame(yolo_model, frame_bgr, conf, device)
        ok, reason = post_tank_detections(session, backend, detections, timeout=2.0)
        if not ok and verbose:
            log.warning("YOLO detections upload failed: %s", reason)
    except Exception as e:
        if verbose:
            log.warning("YOLO inference/upload failed: %s", e)


def run_tank_usb_camera(
    backend: str,
    camera_index: int,
    fps: float,
    verbose: bool,
    yolo_model=None,
    yolo_conf: float = 0.25,
    yolo_device: str | None = None,
    yolo_every_n: int = DEFAULT_YOLO_EVERY_N,
) -> None:
    try:
        import cv2
    except ImportError:
        raise SystemExit("OpenCV is not installed. Run: pip install opencv-python")

    interval = 1.0 / max(fps, 0.1)
    yolo_every_n = max(1, int(yolo_every_n))
    log.info("Tank USB camera: index=%d, target %.1f FPS", camera_index, fps)
    wait_for_backend(backend)
    session = create_http_session()
    upload_failures = 0

    while True:
        cap = cv2.VideoCapture(camera_index, cv2.CAP_DSHOW)
        if not cap.isOpened():
            cap.release()
            cap = cv2.VideoCapture(camera_index)

        if not cap.isOpened():
            log.warning("Cannot open tank USB camera index=%d; retrying in 3s", camera_index)
            time.sleep(3.0)
            continue

        log.info("Tank USB camera opened: index=%d", camera_index)
        last_log_at = 0.0
        frame_no = 0
        try:
            while True:
                started = time.monotonic()
                ok, frame = cap.read()
                if not ok or frame is None:
                    log.warning("Tank USB camera read failed; reconnecting")
                    break
                frame_no += 1

                ok, jpg = cv2.imencode(
                    ".jpg",
                    frame,
                    [int(cv2.IMWRITE_JPEG_QUALITY), JPEG_ENCODE_QUALITY],
                )
                if not ok:
                    log.warning("Tank USB camera JPEG encode failed")
                    time.sleep(interval)
                    continue

                payload = jpg.tobytes()
                posted, reason = post_tank_frame(session, backend, payload, timeout=5.0)
                now = time.monotonic()
                if not posted:
                    upload_failures += 1
                    log.warning("Tank frame upload failed: %s", reason)
                    if upload_failures == 1 or upload_failures % 3 == 0:
                        session.close()
                        session = create_http_session()
                elif verbose and now - last_log_at >= 2.0:
                    upload_failures = 0
                    log.info("Tank frame uploaded: %d bytes", len(payload))
                    last_log_at = now
                elif posted:
                    upload_failures = 0

                if posted and upload_failures == 0 and yolo_model is not None and frame_no % yolo_every_n == 0:
                    maybe_infer_tank_and_post(
                        session, backend, yolo_model, frame, yolo_conf, yolo_device, verbose
                    )

                elapsed = time.monotonic() - started
                if upload_failures >= 3:
                    time.sleep(min(5.0, 0.75 * upload_failures))
                    continue
                time.sleep(max(0.001, interval - elapsed))
        finally:
            cap.release()

        time.sleep(1.0)


def load_yolo_model(weights: str, disabled: bool):
    if disabled:
        return None

    path = Path(weights)
    if not path.is_file():
        log.warning("YOLO weights not found; detection disabled: %s", path)
        return None

    try:
        from ultralytics import YOLO
    except ImportError:
        log.error("ultralytics is not installed; detection disabled. Run: pip install ultralytics")
        return None

    model = YOLO(str(path.resolve()))
    log.info("YOLO loaded: %s", path)
    return model


def main() -> None:
    parser = argparse.ArgumentParser(description="Tank USB camera + YOLO bridge")
    parser.add_argument("--backend", default=DEFAULT_BACKEND, help="Spring Boot backend URL")
    parser.add_argument("--tank-camera-index", type=int, default=DEFAULT_TANK_CAMERA_INDEX, help="USB camera index")
    parser.add_argument("--tank-camera-fps", type=float, default=DEFAULT_CAMERA_FPS, help="Target upload FPS")
    parser.add_argument("--verbose", action="store_true", help="Print successful frame upload logs")
    parser.add_argument("--no-yolo", action="store_true", help="Disable YOLO detection uploads")
    parser.add_argument("--yolo-weights", default=str(_DEFAULT_YOLO_WEIGHTS), help="YOLO .pt weights path")
    parser.add_argument("--yolo-conf", type=float, default=0.25, help="YOLO confidence threshold")
    parser.add_argument("--yolo-device", default="", help="YOLO device: cpu, 0, cuda:0, etc.")
    parser.add_argument("--yolo-every-n", type=int, default=DEFAULT_YOLO_EVERY_N, help="Run YOLO once every N frames")
    args = parser.parse_args()

    yolo_model = load_yolo_model(args.yolo_weights, args.no_yolo)
    yolo_device = args.yolo_device.strip() or None

    try:
        run_tank_usb_camera(
            backend=args.backend,
            camera_index=args.tank_camera_index,
            fps=args.tank_camera_fps,
            verbose=args.verbose,
            yolo_model=yolo_model,
            yolo_conf=args.yolo_conf,
            yolo_device=yolo_device,
            yolo_every_n=args.yolo_every_n,
        )
    except KeyboardInterrupt:
        log.info("Exit")


if __name__ == "__main__":
    main()
