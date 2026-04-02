"""
供 Web 端预览机械臂摄像头：任务线程将当前帧编码为 JPEG 写入共享缓冲，
FastAPI MJPEG 流读取同一缓冲（不单独再打开摄像头）。
"""

from __future__ import annotations

import threading
from typing import Optional

import cv2
import numpy as np

from camera_util import normalize_bgr_frame

_lock = threading.Lock()
_latest_jpg: Optional[bytes] = None


def publish_bgr(frame: np.ndarray, quality: int = 78) -> None:
    """由 tasks 在获取到 BGR 帧后调用（可与 imshow 使用同一幅图）。"""
    frame = normalize_bgr_frame(frame)
    if frame is None:
        return
    ok, buf = cv2.imencode(".jpg", frame, [int(cv2.IMWRITE_JPEG_QUALITY), int(quality)])
    if not ok:
        return
    data = buf.tobytes()
    with _lock:
        global _latest_jpg
        _latest_jpg = data


def get_latest_jpeg() -> Optional[bytes]:
    with _lock:
        return _latest_jpg


def clear() -> None:
    with _lock:
        global _latest_jpg
        _latest_jpg = None
