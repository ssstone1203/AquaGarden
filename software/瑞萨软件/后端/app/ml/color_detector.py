"""
颜色识别检测器
基于 HSV 色彩空间分析，定位指定颜色目标物体
"""
from dataclasses import dataclass
from typing import Optional

import cv2
import numpy as np


@dataclass
class ColorTarget:
    """检测到的颜色目标"""
    color: str
    center_x: int
    center_y: int
    area: float
    confidence: float
    hsv_range: tuple  # (lower, upper)
    world_x: float
    world_y: float
    world_z: float


class ColorDetector:
    """
    HSV 颜色空间目标检测器。
    支持红、绿、蓝、黄等多种颜色的检测。
    """

    # 预定义颜色 HSV 阈值
    HSV_RANGES: dict[str, tuple[tuple, tuple]] = {
        # OpenCV HSV: H [0-180], S [0-255], V [0-255]
        "red": (  # 红色跨越 H=0 和 H=180，需要分两段
            [(np.array([0, 100, 100]), np.array([10, 255, 255])),
             (np.array([170, 100, 100]), np.array([180, 255, 255]))]
        ),
        "green":    ((np.array([35, 100, 100]),  np.array([85, 255, 255]))),
        "blue":     ((np.array([100, 100, 100]), np.array([130, 255, 255]))),
        "yellow":   ((np.array([15, 100, 100]),  np.array([35, 255, 255]))),
        "orange":   ((np.array([5, 100, 100]),   np.array([15, 255, 255]))),
        "purple":   ((np.array([130, 100, 100]), np.array([160, 255, 255]))),
        "white":    ((np.array([0, 0, 200]),     np.array([180, 30, 255]))),
        "black":    ((np.array([0, 0, 0]),       np.array([180, 255, 30]))),
    }

    def __init__(self, min_area: int = 500):
        self.min_area = min_area

    def detect(
        self,
        frame: np.ndarray,
        target_color: str,
        depth_image: Optional[np.ndarray] = None,
    ) -> list[ColorTarget]:
        """
        在给定帧中检测指定颜色目标。

        Args:
            frame: BGR 格式图像（OpenCV 读取的原始图像）
            target_color: 目标颜色名（red/green/blue/yellow/orange/purple/white/black）
            depth_image: 可选深度图像（与 frame 同尺寸，单位 mm）

        Returns:
            list[ColorTarget]: 检测到的目标列表
        """
        targets: list[ColorTarget] = []

        if target_color not in self.HSV_RANGES:
            return targets

        # BGR → HSV
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

        ranges = self.HSV_RANGES[target_color]

        # 构建掩膜
        if isinstance(ranges, list):
            mask = np.zeros(hsv.shape[:2], dtype=np.uint8)
            for lower, upper in ranges:
                mask |= cv2.inRange(hsv, lower, upper)
        else:
            lower, upper = ranges
            mask = cv2.inRange(hsv, lower, upper)

        # 形态学去噪
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)

        # 找轮廓
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        for contour in contours:
            area = cv2.contourArea(contour)
            if area < self.min_area:
                continue

            # 计算中心
            M = cv2.moments(contour)
            if M["m00"] == 0:
                continue
            cx = int(M["m10"] / M["m00"])
            cy = int(M["m01"] / M["m00"])

            # 深度信息
            wx, wy, wz = self._pixel_to_world(cx, cy, depth_image)

            # 置信度（基于颜色饱和度/亮度）
            h_val, s_val, v_val = hsv[cy, cx]
            confidence = min(1.0, float(s_val) / 200.0)

            targets.append(ColorTarget(
                color=target_color,
                center_x=cx,
                center_y=cy,
                area=area,
                confidence=round(confidence, 3),
                hsv_range=(ranges[0] if not isinstance(ranges, list) else ranges[0],
                           ranges[1] if not isinstance(ranges, list) else ranges[1]),
                world_x=round(wx, 1),
                world_y=round(wy, 1),
                world_z=round(wz, 1),
            ))

        # 按面积降序排列
        targets.sort(key=lambda t: t.area, reverse=True)
        return targets

    @staticmethod
    def _pixel_to_world(
        px: int,
        py: int,
        depth_image: Optional[np.ndarray],
    ) -> tuple[float, float, float]:
        """
        将像素坐标转换为世界坐标（深度辅助）。
        生产环境需要相机内参和位姿进行坐标变换。
        """
        if depth_image is not None and 0 <= py < depth_image.shape[0] and 0 <= px < depth_image.shape[1]:
            z = float(depth_image[py, px])
            # 简化：假设焦距 fx=fy=500，中心 cx=320, cy=240
            fx = fy = 500.0
            cx, cy = 320.0, 240.0
            x = (px - cx) * z / fx
            y = (py - cy) * z / fy
            return x, y, z
        return 0.0, 0.0, 183.0  # 默认深度 183mm
