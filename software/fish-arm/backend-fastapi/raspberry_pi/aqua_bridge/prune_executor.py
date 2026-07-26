#!/usr/bin/env python3
# encoding: utf-8
"""
视觉裁剪 — 五区手掰位姿：

  1. YOLO 分割检出黄叶 → 掩膜中心 (gx,gy) + 深度中值 (mm)；
  2. 与标定五点 (左近/右近/中间/左远/右远) 最近邻 → 区域；
  3. 直接下发该区手掰脉宽（接近时夹爪张开，到位后按该区 1 号脉宽闭合）。
"""

from __future__ import annotations

import os
import threading
import time
from typing import Dict, Optional, Tuple

import cv2
import numpy as np
import yaml

from aqua_bridge.prune_yolo import (
    detect_stem_grasp,
    load_yolo_model,
)

PRUNE_OBSERVE_POSE = {1: 220, 2: 489, 3: 130, 4: 842, 5: 836, 6: 509}
GRIPPER_OPEN = 220
DEFAULT_PREPARE_POSE = {1: 223, 2: 489, 3: 324, 4: 1044, 5: 833, 6: 507}

# 新鱼缸标定：黄叶中心横坐标区分左右，掩膜高度区分远近。
ZONE_ORDER = ('中间', '右近', '左近', '右远', '左远')
ZONE_FEATURE_SCALE = (100.0, 50.0)
ZONE_MAX_DISTANCE_SQ = 2.0

ZONE_POSE_YAML_PATHS = (
    '/home/ubuntu/AquaGarden/config/prune_zone_poses.yaml',
    '/home/pi/Desktop/prune_calibration/manual_poses.yaml',
)

# 内置手掰位姿（YAML 缺失时使用）
DEFAULT_ZONE_POSES: Dict[str, Dict[int, int]] = {
    '中间': {1: 539, 2: 483, 3: 605, 4: 924, 5: 444, 6: 515},
    '右近': {1: 547, 2: 477, 3: 532, 4: 1040, 5: 615, 6: 421},
    '左近': {1: 513, 2: 487, 3: 619, 4: 1007, 5: 523, 6: 603},
    '右远': {1: 557, 2: 483, 3: 566, 4: 714, 5: 299, 6: 452},
    '左远': {1: 505, 2: 478, 3: 562, 4: 755, 5: 329, 6: 570},
}

DEFAULT_ZONE_REFS: Dict[str, Tuple[int, int]] = {
    '中间': (346, 200),
    '右近': (512, 268),
    '左近': (227, 232),
    '右远': (445, 163),
    '左远': (267, 164),
}

PREVIEW_SAVE_DIRS = (
    '/home/pi/Desktop/aqua_prune_debug',
    '/home/ubuntu/AquaGarden/log/prune_preview',
)


def _parse_pose(raw, fallback: Dict[int, int]) -> Dict[int, int]:
    pose = dict(fallback)
    pose.update({int(k): int(v) for k, v in (raw or {}).items()})
    if set(pose) != set(range(1, 7)):
        raise ValueError('servo pose must contain IDs 1..6')
    for sid, pulse in pose.items():
        if pulse < 0 or pulse > 1200:
            raise ValueError(f'servo {sid} pulse out of calibrated range: {pulse}')
    return pose


def _load_zone_table():
    poses = {k: dict(v) for k, v in DEFAULT_ZONE_POSES.items()}
    refs = dict(DEFAULT_ZONE_REFS)
    prepare_pose = dict(DEFAULT_PREPARE_POSE)
    scale = ZONE_FEATURE_SCALE
    max_distance_sq = ZONE_MAX_DISTANCE_SQ
    for path in ZONE_POSE_YAML_PATHS:
        if not os.path.isfile(path):
            continue
        with open(path, 'r', encoding='utf-8') as f:
            data = yaml.safe_load(f) or {}
        prepare_pose = _parse_pose(data.get('prepare_pose'), prepare_pose)
        raw_scale = data.get('feature_scale') or {}
        scale = (
            float(raw_scale.get('gx', scale[0])),
            float(raw_scale.get('leaf_height', scale[1])),
        )
        if scale[0] <= 0 or scale[1] <= 0:
            raise ValueError('feature_scale values must be positive')
        max_distance_sq = float(data.get('max_distance_sq', max_distance_sq))
        for item in data.get('calibration', []):
            zid = str(item.get('id', '')).strip()
            if zid not in ZONE_ORDER:
                continue
            poses[zid] = _parse_pose(item.get('servo'), poses[zid])
            ref = item.get('ref')
            if ref and 'gx' in ref and 'leaf_height' in ref:
                refs[zid] = (int(ref['gx']), int(ref['leaf_height']))
        break
    return poses, refs, prepare_pose, scale, max_distance_sq


def classify_zone(
    gx: int,
    leaf_height: float,
    refs: Dict[str, Tuple[int, int]],
    scale=ZONE_FEATURE_SCALE,
) -> Tuple[str, float]:
    """最近邻：黄叶横向位置 + 掩膜高度映射到五区之一。"""
    sx, sh = scale
    best_id = ZONE_ORDER[0]
    best_d = 1e18
    for zid in ZONE_ORDER:
        rx, rh = refs[zid]
        d = ((gx - rx) / sx) ** 2 + ((leaf_height - rh) / sh) ** 2
        if d < best_d:
            best_d = d
            best_id = zid
    return best_id, float(best_d)


def select_zone(
    gx: int,
    leaf_height: float,
    refs: Dict[str, Tuple[int, int]],
    scale=ZONE_FEATURE_SCALE,
    max_distance_sq=ZONE_MAX_DISTANCE_SQ,
) -> Tuple[Optional[str], float]:
    zone_id, distance = classify_zone(gx, leaf_height, refs, scale=scale)
    if distance > max_distance_sq:
        return None, distance
    return zone_id, distance


class PruneExecutor:
    """HOME 观测 → YOLO 五区分类 → 手掰脉宽夹取。"""

    def __init__(self, node, model_path: str = ''):
        self._node = node
        self._yolo = None
        self._yolo_lock = threading.Lock()
        self._model_path = model_path
        (
            self._zone_poses,
            self._zone_refs,
            self._prepare_pose,
            self._zone_scale,
            self._zone_max_distance_sq,
        ) = _load_zone_table()
        self._last_zone: dict = {}
        self._node.get_logger().info(
            f'[prune] 五区手掰模式，位姿表: {list(self._zone_poses.keys())}'
        )

    def _get_yolo(self):
        with self._yolo_lock:
            if self._yolo is None:
                self._node.get_logger().info('加载黄叶分割模型...')
                self._yolo = load_yolo_model(self._model_path)
            return self._yolo

    def _snapshot(self):
        with self._node._camera_lock:
            rgb = None if self._node._last_rgb is None else self._node._last_rgb.copy()
            depth = None if self._node._last_depth is None else self._node._last_depth.copy()
            k = None if self._node._rgb_K is None else self._node._rgb_K.copy()
        return rgb, depth, k

    def _wait_snapshot(self, timeout_sec=10.0):
        deadline = time.time() + timeout_sec
        while time.time() < deadline:
            rgb, depth, k = self._snapshot()
            if rgb is not None:
                return rgb, depth, k
            self._sleep_with_stop(0.15)
        return self._snapshot()

    def _current_servo_pose(self):
        return dict(getattr(self._node, '_last_servo_pose', PRUNE_OBSERVE_POSE))

    def _log_pose(self, tag: str):
        cur = self._current_servo_pose()
        self._node.get_logger().info(
            f'[prune] {tag} 脉宽: '
            f'1={cur.get(1)} 2={cur.get(2)} 3={cur.get(3)} '
            f'4={cur.get(4)} 5={cur.get(5)} 6={cur.get(6)}'
        )

    def _sleep_with_stop(self, duration: float):
        deadline = time.time() + float(duration)
        while time.time() < deadline:
            self._node._check_stop()
            time.sleep(min(0.05, max(0.0, deadline - time.time())))

    def _publish_servos(self, duration, positions, step: str = ''):
        self._node._check_stop()
        positions = tuple(positions)
        parts = ', '.join(f'{sid}→{pulse}' for sid, pulse in positions)
        label = f' [{step}]' if step else ''
        self._node.get_logger().info(f'[prune] 下发{label} dur={duration}s: {parts}')
        self._node._move_servo(positions, float(duration))

    def _annotate_debug(
        self, vis: np.ndarray, gx: int, gy: int, leaf_height: int,
        zone_id: str, zone_pose: dict,
    ) -> np.ndarray:
        cv2.drawMarker(vis, (gx, gy), (0, 0, 255), cv2.MARKER_CROSS, 14, 2)
        cv2.putText(
            vis, f'ZONE={zone_id} x={gx} height={leaf_height}', (10, 28),
            cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 255, 255), 2, cv2.LINE_AA,
        )
        cv2.putText(
            vis,
            ' '.join(f'{i}:{zone_pose[i]}' for i in range(1, 7)),
            (10, 54), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (200, 255, 200), 1, cv2.LINE_AA,
        )
        return vis

    def _observe_target(self):
        rgb, _depth, _k = self._wait_snapshot()
        if rgb is None:
            self._node.get_logger().warn('[prune] RGB 相机未就绪（等待超时）')
            return None

        det = detect_stem_grasp(self._get_yolo(), rgb)
        if det is None or det.leaf is None:
            self._node.get_logger().warn('[prune] YOLO 未检出黄叶')
            self._save_preview_image(rgb, 'no_yolo')
            return None

        gx, gy = det.leaf.center
        _x, _y, _width, leaf_height = cv2.boundingRect(
            det.leaf.polygon.astype(np.int32)
        )
        zone_id, dist = select_zone(
            gx,
            leaf_height,
            self._zone_refs,
            scale=self._zone_scale,
            max_distance_sq=self._zone_max_distance_sq,
        )
        if zone_id is None:
            self._node.get_logger().warn(
                f'[prune] 黄叶超出标定范围: x={gx} height={leaf_height} d²={dist:.3f}'
            )
            cv2.putText(
                det.debug_bgr,
                f'OUT OF RANGE d2={dist:.2f}',
                (10, 54),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.65,
                (0, 0, 255),
                2,
                cv2.LINE_AA,
            )
            self._save_preview_image(det.debug_bgr, 'out_of_range')
            return None

        zone_pose = dict(self._zone_poses[zone_id])
        self._last_zone = {
            'zone': zone_id, 'dist': dist, 'gx': gx, 'gy': gy,
            'leaf_height': leaf_height,
            'grasp_gx': det.gx, 'grasp_gy': det.gy,
        }
        self._node.get_logger().info(
            f'[prune] 黄叶中心=({gx},{gy}) 高度={leaf_height}px '
            f'视觉点=({det.gx},{det.gy}) → 区域={zone_id} (d²={dist:.3f}) '
            f'脉宽={zone_pose}'
        )
        vis = self._annotate_debug(
            det.debug_bgr, gx, gy, leaf_height, zone_id, zone_pose
        )
        self._save_preview_image(vis, f'zone_{zone_id}')
        return zone_id, zone_pose, rgb

    def _save_preview_image(self, vis: np.ndarray, tag: str) -> str:
        ts = time.strftime('%Y%m%d_%H%M%S')
        name = f'{ts}_{tag}.jpg'
        for root in PREVIEW_SAVE_DIRS:
            try:
                os.makedirs(root, exist_ok=True)
                path = os.path.join(root, name)
                cv2.imwrite(path, vis)
                self._node.get_logger().info(f'[prune] 可视化: {path}')
                return path
            except OSError as exc:
                self._node.get_logger().warn(f'保存失败 {root}: {exc}')
        return ''

    def _execute_zone_grasp(self, zone_id: str, zone_pose: dict):
        """准备位姿 → 目标位姿(夹爪保持张开) → 使用该区脉宽闭合。"""
        approach = {sid: int(zone_pose[sid]) for sid in range(1, 7)}
        approach[1] = int(self._prepare_pose[1])
        close_pulse = int(zone_pose[1])
        self._node.get_logger().info(
            f'[prune] 执行区域={zone_id} 准备位姿={self._prepare_pose} '
            f'目标2~6={{{", ".join(f"{i}:{approach[i]}" for i in range(2,7))}}} '
            f'夹爪闭合→{close_pulse}'
        )
        self._log_pose('准备前')
        self._publish_servos(
            1.3,
            tuple((sid, self._prepare_pose[sid]) for sid in range(1, 7)),
            step='准备夹取',
        )
        self._sleep_with_stop(0.35)
        self._publish_servos(
            1.3,
            tuple((sid, approach[sid]) for sid in range(1, 7)),
            step=f'{zone_id}-到位(1号张开)',
        )
        self._sleep_with_stop(0.35)
        for i in range(2):
            self._publish_servos(
                1.0,
                ((1, close_pulse),),
                step=f'{zone_id}-夹爪闭合{i + 1}/2',
            )
            self._sleep_with_stop(0.12)
        self._sleep_with_stop(0.65)
        self._log_pose('夹取后')
        return True

    def preview_inference_only(self) -> tuple[bool, str]:
        self._node.get_logger().info('[prune] 预览：YOLO 五区分类，机械臂不动')
        rgb, _depth, _ = self._snapshot()
        if rgb is None:
            raise RuntimeError('相机 RGB 未就绪')

        det = detect_stem_grasp(self._get_yolo(), rgb)
        if det is None or det.leaf is None:
            vis = rgb.copy()
            cv2.putText(vis, 'no yellow leaf', (10, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
            return False, self._save_preview_image(vis, 'no_target')

        gx, gy = det.leaf.center
        _x, _y, _width, leaf_height = cv2.boundingRect(
            det.leaf.polygon.astype(np.int32)
        )
        zone_id, dist = select_zone(
            gx,
            leaf_height,
            self._zone_refs,
            scale=self._zone_scale,
            max_distance_sq=self._zone_max_distance_sq,
        )
        if zone_id is None:
            cv2.putText(
                det.debug_bgr, f'OUT OF RANGE d2={dist:.2f}', (10, 54),
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2,
            )
            return False, self._save_preview_image(det.debug_bgr, 'out_of_range')
        vis = self._annotate_debug(
            det.debug_bgr, gx, gy, leaf_height, zone_id, self._zone_poses[zone_id]
        )
        return True, self._save_preview_image(vis, f'preview_{zone_id}')

    def run_once(self) -> bool:
        self._node._check_stop()

        self._log_pose('观测前')
        self._publish_servos(0.45, ((1, GRIPPER_OPEN),), step='张开夹爪')
        self._sleep_with_stop(0.5)
        self._node.get_logger().info('[prune] HOME：YOLO RGB 五区分类 → 标定位姿')

        observed = None
        for attempt in range(6):
            observed = self._observe_target()
            if observed is not None:
                break
            self._node.get_logger().info(f'[prune] 观测重试 {attempt + 1}/6...')
            self._sleep_with_stop(0.8)

        if observed is None:
            return False

        zone_id, zone_pose, _rgb = observed
        return self._execute_zone_grasp(zone_id, zone_pose)
