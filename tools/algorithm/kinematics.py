"""
tools/algorithm/kinematics.py
Python 版运动学库，与 hardware/arm/app/algorithm/kinematics.c 逻辑完全一致。

对外接口：
  pitch_range_solve(x, y, z, pitch, min_pitch, max_pitch, step=2.0)
      → (j0, j1, j2, j3) 或 None

  kin_forward_matrix(j0, j1, j2, j3)
      → 4×4 numpy 变换矩阵（末端在基座坐标系中的位姿）
"""

import math
import numpy as np

# ── 连杆参数（与 kinematics.h 完全一致，单位 cm）──────────────────
L1, L2, L3, L4 = 2.89, 10.43, 8.9, 17.7

# ── 关节角度限制（与 kinematics.h 完全一致，单位 °）──────────────
J_LIM = [
    (-90.0,  90.0),   # joint0 基座旋转
    (  0.0, 180.0),   # joint1 肩部俯仰
    (-150.0, 150.0),  # joint2 肘部俯仰
    (-100.0,  90.0),  # joint3 腕部俯仰
]


def _ik_single(x: float, y: float, z: float, pitch: float):
    """
    单 pitch 值逆运动学（修复版，对应修复后的 kinematics.c）。
    成功返回 (j0, j1, j2, j3)（度），失败返回 None。
    """
    r  = math.sqrt(x*x + y*y)
    j0 = math.degrees(math.atan2(y, x)) if r > 1e-3 else 0.0

    ar = math.radians(pitch)
    a  = r  - L4 * math.cos(ar)
    b  = z  - L1 - L4 * math.sin(ar)

    # 腕部距离检查（修复：使用腕部到肩部的距离，而非原点到末端距离）
    wd2 = a*a + b*b
    if wd2 > (L2 + L3)**2 or wd2 < (abs(L2 - L3))**2:
        return None

    cos2 = (wd2 - L2*L2 - L3*L3) / (2.0 * L2 * L3)
    cos2 = max(-1.0, min(1.0, cos2))          # 限幅，防止浮点溢出
    sin2 = -math.sqrt(1.0 - cos2 * cos2)      # elbow-up

    j2 = math.degrees(math.atan2(sin2, cos2))

    c  = L2 + L3 * cos2
    d  = L3 * sin2
    j1 = math.degrees(math.atan2(c, d) - math.atan2(a, b))
    j3 = pitch - j1 - j2

    # 关节限位检查
    for ang, (lo, hi) in zip([j0, j1, j2, j3], J_LIM):
        if not (lo <= ang <= hi):
            return None

    return (j0, j1, j2, j3)


def pitch_range_solve(x: float, y: float, z: float,
                      pitch: float,
                      min_pitch: float, max_pitch: float,
                      step: float = 2.0):
    """
    在 [pitch→min_pitch] 和 [pitch→max_pitch] 两段范围内以 step° 步长扫描，
    返回第一个有效的关节角四元组 (j0, j1, j2, j3)（度），失败返回 None。

    与修复后的 C 版 PitchRange_Set 逻辑一致。
    """
    for lo, hi in [(min(pitch, min_pitch), max(pitch, min_pitch)),
                   (min(pitch, max_pitch), max(pitch, max_pitch))]:
        p = lo
        while p <= hi + 0.01:
            res = _ik_single(x, y, z, p)
            if res is not None:
                return res
            p += step
        # 补充检查精确边界
        res = _ik_single(x, y, z, hi)
        if res is not None:
            return res
    return None


def kin_forward_matrix(j0: float, j1: float, j2: float, j3: float) -> np.ndarray:
    """
    正向运动学：根据关节角（度）计算末端 4×4 齐次变换矩阵。
    坐标系与 C 版 Kin_Forward 完全一致。

    矩阵含义：T_end2base，将末端坐标系的点变换到基座坐标系。
    """
    t0   = math.radians(j0)
    t1   = math.radians(j1)
    t12  = t1 + math.radians(j2)
    t123 = t12 + math.radians(j3)

    # 末端执行器在基座坐标系中的位置
    hor = L2*math.cos(t1) + L3*math.cos(t12) + L4*math.cos(t123)
    pz  = L1 + L2*math.sin(t1) + L3*math.sin(t12) + L4*math.sin(t123)
    px  = hor * math.cos(t0)
    py  = hor * math.sin(t0)

    # 末端姿态（俯仰角 = j1+j2+j3）
    alpha = math.radians(j1 + j2 + j3)
    ca, sa = math.cos(alpha), math.sin(alpha)
    c0, s0 = math.cos(t0),    math.sin(t0)

    # 末端坐标系三轴在基座坐标系中的方向
    z_ee = np.array([ ca*c0,  ca*s0,  sa])   # 工具轴（沿俯仰方向）
    y_ee = np.array([-s0,     c0,     0.0])   # 侧向轴
    x_ee = np.cross(y_ee, z_ee)               # 法向轴

    T = np.eye(4)
    T[:3, 0] = x_ee
    T[:3, 1] = y_ee
    T[:3, 2] = z_ee
    T[:3, 3] = [px, py, pz]
    return T
