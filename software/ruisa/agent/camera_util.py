"""
机械臂 USB 摄像头：与笔记本内置摄像头区分。

- 优先使用设备路径（Linux /dev/video*，或 OpenCV 支持的路径）
- 否则使用序号 ARM_CAMERA_INDEX / CAMERA_INDEX（环境变量），默认 1（常见：内置为 0、臂载 USB 为 1）
- Windows 可用 CAMERA_BACKEND=dshow | msmf | v4l2 | auto；auto 时与 tasks 一致优先 DirectShow
"""

from __future__ import annotations

import sys
from typing import Any, Optional

import cv2
import numpy as np

import config


def normalize_bgr_frame(frame: Any) -> Optional[np.ndarray]:
    """
    将摄像头读出的 ndarray 转为 OpenCV 可安全使用的连续 uint8 BGR。
    部分驱动返回非连续或异常步长时，cv::Mat 会触发 _step >= minstep 断言失败。
    """
    if frame is None or getattr(frame, "size", 0) == 0:
        return None
    if frame.ndim != 3 or int(frame.shape[2]) != 3:
        return None
    h, w = int(frame.shape[0]), int(frame.shape[1])
    if h < 2 or w < 2:
        return None
    if frame.dtype != np.uint8:
        frame = np.asarray(frame, dtype=np.uint8)
    if not frame.flags.get("C_CONTIGUOUS", False):
        frame = np.ascontiguousarray(frame)
    return frame


def _opencv_capture_api() -> Optional[int]:
    """按平台与 config.CAMERA_BACKEND 选择 VideoCapture 第二参数；None 表示单参数构造。"""
    be = (getattr(config, "CAMERA_BACKEND", "auto") or "auto").lower()
    if sys.platform == "win32":
        if be in ("dshow", "auto"):
            return cv2.CAP_DSHOW
        if be == "msmf":
            return cv2.CAP_MSMF
        if be == "v4l2" and hasattr(cv2, "CAP_V4L2"):
            return cv2.CAP_V4L2
        return None
    if be == "v4l2":
        return cv2.CAP_V4L2
    return None


def try_open_camera(
    width: int = 1280,
    height: int = 720,
    buffer_size: Optional[int] = None,
) -> tuple[Optional[cv2.VideoCapture], Optional[object]]:
    """
    供 tasks 使用：优先 ARM_CAMERA_DEVICE；否则按 CAMERA_INDEX 并回退尝试 0/1/2。
    成功返回 (cap, 标识)；标识为设备路径 str 或索引 int。失败 (None, None)。
    """
    path = getattr(config, "ARM_CAMERA_DEVICE", None)
    if path:
        cap = cv2.VideoCapture(path)
        if not cap.isOpened():
            cap.release()
            return None, None
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        if buffer_size is not None:
            cap.set(cv2.CAP_PROP_BUFFERSIZE, buffer_size)
        ok = False
        for _ in range(20):
            ret, raw = cap.read()
            if ret and normalize_bgr_frame(raw) is not None:
                ok = True
                break
        if ok:
            return cap, path
        cap.release()
        return None, None

    primary = int(getattr(config, "CAMERA_INDEX", 1))
    candidates = [primary] + [i for i in (0, 1, 2) if i != primary]
    api = _opencv_capture_api()
    for idx in candidates:
        cap = (
            cv2.VideoCapture(idx, api) if api is not None else cv2.VideoCapture(idx)
        )
        if not cap.isOpened():
            cap.release()
            continue
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        if buffer_size is not None:
            cap.set(cv2.CAP_PROP_BUFFERSIZE, buffer_size)
        ok = False
        for _ in range(20):
            ret, raw = cap.read()
            if ret and normalize_bgr_frame(raw) is not None:
                ok = True
                break
        if ok:
            return cap, idx
        cap.release()
    return None, None


def open_arm_camera(width: int = 1280, height: int = 720, buffer_size: Optional[int] = None):
    """
    打开配置中的「机械臂摄像头」，失败时抛出 RuntimeError。
    不做索引回退；需要回退时请用 try_open_camera。
    """
    path = getattr(config, "ARM_CAMERA_DEVICE", None)
    if path:
        cap = cv2.VideoCapture(path)
    else:
        idx = int(getattr(config, "CAMERA_INDEX", 1))
        api = _opencv_capture_api()
        cap = cv2.VideoCapture(idx, api) if api is not None else cv2.VideoCapture(idx)

    if not cap.isOpened():
        hint = (
            f"path={path!r} index={getattr(config, 'CAMERA_INDEX', 1)} "
            f"backend={getattr(config, 'CAMERA_BACKEND', 'auto')}"
        )
        raise RuntimeError(
            "无法打开机械臂摄像头（请确认 USB 接在机械臂相机上）。可设置环境变量："
            "ARM_CAMERA_DEVICE（设备路径）或 ARM_CAMERA_INDEX / CAMERA_INDEX（序号），"
            "Windows 可试 CAMERA_BACKEND=dshow。"
            f" 当前：{hint}"
        )

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    if buffer_size is not None:
        cap.set(cv2.CAP_PROP_BUFFERSIZE, buffer_size)
    return cap
