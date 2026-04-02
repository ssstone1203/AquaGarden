"""
颜色检测服务
封装 OpenCV HSV 检测 + 像素坐标到机械臂坐标的仿射变换
"""
import json
import time
from datetime import datetime, timezone
from typing import Optional

import numpy as np

from app.ml.color_detector import ColorDetector


# ── 分拣放置区域坐标（基于 clamp.py）─────────────────────────────────────────
SORT_ZONES: dict[str, dict] = {
    "red":   {"x": 8.31,  "y": 21.78, "z": -6.54, "pitch": -74.2},
    "green": {"x": 0.84,  "y": 22.21, "z": -7.01, "pitch": -78.0},
    "blue":  {"x": -4.36, "y": 22.34, "z": -7.24, "pitch": -72.2},
}


class ColorService:
    """
    颜色检测 + 坐标转换服务。
    核心逻辑对应 clamp.py 中的 detect_block() 和 pixel_to_arm()。
    """

    def __init__(self):
        self._detector = ColorDetector(min_area=500)
        self._calibration: Optional[dict] = None

    def load_calibration(self, affine_matrix: list[float], obs_position: dict) -> None:
        """
        加载标定数据（仿射矩阵 + 观测位姿）。
        affine_matrix: 4×3 行优先，共12个 float
        """
        self._calibration = {
            "A": np.array(affine_matrix, dtype=np.float32).reshape(4, 3),
            "obs": obs_position,
        }

    def detect_and_convert(
        self,
        frame: np.ndarray,
        colors: list[str] | None = None,
        depth_image: Optional[np.ndarray] = None,
    ) -> list[dict]:
        """
        在帧中检测指定颜色，并将像素坐标转换为机械臂坐标。

        Args:
            frame: BGR 格式图像（OpenCV）
            colors: 待检测颜色列表，默认为 red/green/blue
            depth_image: 可选深度图像（mm）

        Returns:
            list[dict]: 每个检测到的目标包含像素坐标和转换后的机械臂坐标
        """
        if colors is None:
            colors = ["red", "green", "blue"]

        results: list[dict] = []
        obs_pos = self._calibration["obs"] if self._calibration else {
            "x": 16.0, "y": 0.0, "z": -3.2, "pitch": -76.1
        }

        for color in colors:
            targets = self._detector.detect(frame, color, depth_image)
            for target in targets:
                arm_pos = self._pixel_to_arm(
                    target.center_x,
                    target.center_y,
                    obs_pos,
                )
                results.append({
                    "color": color,
                    "pixel_x": target.center_x,
                    "pixel_y": target.center_y,
                    "confidence": target.confidence,
                    "area": target.area,
                    "arm_x": arm_pos[0],
                    "arm_y": arm_pos[1],
                    "arm_z": arm_pos[2],
                    "arm_pitch": arm_pos[3],
                })

        return results

    def _pixel_to_arm(
        self,
        u: int,
        v: int,
        obs_pos: dict,
    ) -> tuple[float, float, float, float]:
        """
        像素坐标 → 机械臂坐标。
        使用仿射变换矩阵：
          [x, y, z, pitch]^T ≈ A @ [u, v, 1]^T
        结果在观测位姿坐标系下，加上观测位偏移得到世界坐标。
        """
        if self._calibration is not None:
            A = self._calibration["A"]
            pixel = np.array([u, v, 1.0], dtype=np.float32)
            arm_pos = A @ pixel  # shape (4,)

            # 加观测位偏移
            wx = float(arm_pos[0]) + obs_pos["x"]
            wy = float(arm_pos[1]) + obs_pos["y"]
            wz = float(arm_pos[2]) + obs_pos["z"]
            wpitch = float(arm_pos[3]) + obs_pos["pitch"]
            return (wx, wy, wz, wpitch)

        # 无标定时返回观测位坐标（安全默认值）
        return (
            obs_pos["x"],
            obs_pos["y"],
            obs_pos["z"],
            obs_pos["pitch"],
        )

    @staticmethod
    def get_sort_zone(color: str) -> dict:
        """获取指定颜色的分拣放置区域坐标"""
        return SORT_ZONES.get(color, SORT_ZONES["red"])
