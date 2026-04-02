"""
机械臂 USB 摄像头：与笔记本内置摄像头区分。

- 优先使用设备路径（Linux /dev/video*，或 OpenCV 支持的路径）
- 否则使用序号 ARM_CAMERA_INDEX / CAMERA_INDEX（环境变量），默认 1（常见：内置为 0、臂载 USB 为 1）
- Windows 可用 CAMERA_BACKEND=dshow 指定 DirectShow，便于稳定选中 USB 相机
"""

from __future__ import annotations

from typing import Optional

import cv2

import config


def open_arm_camera(width: int = 1280, height: int = 720, buffer_size: Optional[int] = None):
    """
    打开配置中的「机械臂摄像头」，失败时抛出 RuntimeError。
    """
    path = getattr(config, "ARM_CAMERA_DEVICE", None)
    if path:
        cap = cv2.VideoCapture(path)
    else:
        idx = int(getattr(config, "CAMERA_INDEX", 1))
        be = (getattr(config, "CAMERA_BACKEND", "auto") or "auto").lower()
        api = 0
        if be == "dshow":
            api = cv2.CAP_DSHOW
        elif be == "msmf":
            api = cv2.CAP_MSMF
        elif be == "v4l2":
            api = cv2.CAP_V4L2
        cap = cv2.VideoCapture(idx, api) if api else cv2.VideoCapture(idx)

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
