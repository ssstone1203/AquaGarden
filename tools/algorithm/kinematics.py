"""
Python版运动学模块 - 与 hardware/arm/app/algorithm/kinematics.c 完全对应

连杆参数（单位：cm）:
  L1 = 2.89  基座高度/水平偏移
  L2 = 10.43 大臂
  L3 = 8.90  小臂
  L4 = 17.7  腕部/末端杆
"""

import numpy as np

# ── 连杆长度（cm） ──────────────────────────────────────────────
L1 = 2.89
L2 = 10.43
L3 = 8.90
L4 = 17.7

# ── 关节角度限制（度） ─────────────────────────────────────────
JOINT_LIMITS = [
    (-90,  90),    # 关节0 基座旋转
    (-90, 180),    # 关节1 肩部俯仰
    (-150, 150),   # 关节2 肘部俯仰
    (-150, 150),   # 关节3 腕部俯仰
]


def kin_forward(j0, j1, j2, j3):
    """
    正运动学：给定4个关节角度（度），返回末端坐标 (x, y, z)（cm）
    与 kinematics.c 中 Kin_Forward() 完全对应
    """
    r0, r1, r2, r3 = np.deg2rad([j0, j1, j2, j3])

    base_x = L1
    shoulder_x = base_x + L2 * np.cos(r1)
    shoulder_z = L1    + L2 * np.sin(r1)

    elbow_x = shoulder_x + L3 * np.cos(r1 + r2)
    elbow_z = shoulder_z + L3 * np.sin(r1 + r2)

    wrist_x = elbow_x + L4 * np.cos(r1 + r2 + r3)
    wrist_z = elbow_z + L4 * np.sin(r1 + r2 + r3)

    end_x = wrist_x * np.cos(r0)
    end_y = wrist_x * np.sin(r0)
    end_z = wrist_z

    return float(end_x), float(end_y), float(end_z)


def kin_forward_matrix(j0, j1, j2, j3):
    """
    正运动学：返回末端相对于基座的 4x4 齐次变换矩阵 T_end2base
    位置单位：cm
    旋转约定：R = Rz(θ0) * Ry(θ1+θ2+θ3)
    """
    r0 = np.deg2rad(j0)
    alpha = np.deg2rad(j1 + j2 + j3)   # 末端总俯仰角

    x, y, z = kin_forward(j0, j1, j2, j3)

    # 旋转矩阵（列 = 末端坐标系在基座坐标系下的各轴方向）
    R = np.array([
        [ np.cos(r0)*np.cos(alpha), -np.sin(r0),  np.cos(r0)*np.sin(alpha)],
        [ np.sin(r0)*np.cos(alpha),  np.cos(r0),  np.sin(r0)*np.sin(alpha)],
        [-np.sin(alpha),             0,            np.cos(alpha)           ],
    ])

    T = np.eye(4)
    T[:3, :3] = R
    T[:3,  3] = [x, y, z]
    return T


def kin_inverse(x, y, z, pitch_deg):
    """
    逆运动学：给定末端坐标(cm)和俯仰角(度)，返回 (j0,j1,j2,j3) 或 None
    与 kinematics.c 中 Kin_Inverse() 完全对应（ELBOW_UP 模式）
    """
    alpha = np.deg2rad(pitch_deg)
    len_xy = np.sqrt(x*x + y*y)

    # 关节0 基座旋转
    j0_rad = 0.0 if len_xy < 0.001 else np.arctan2(y, x)

    # 工作空间检查
    dist = np.sqrt(len_xy**2 + z**2)
    if dist > L2 + L3 + L4 or dist < abs(L2 - L3 - L4):
        return None

    # 去掉末端杆L4后的腕部位置
    a = len_xy - L4 * np.cos(alpha)
    b = z - L1  - L4 * np.sin(alpha)

    # 关节2 肘部（余弦定理）
    cos_j2 = (a*a + b*b - L2*L2 - L3*L3) / (2.0 * L2 * L3)
    if abs(cos_j2) > 1.0:
        return None
    sin_j2 = -np.sqrt(1.0 - cos_j2**2)   # ELBOW_UP
    j2_rad = np.arctan2(sin_j2, cos_j2)

    # 关节1 肩部
    c = L2 + L3 * cos_j2
    d = L3 * sin_j2
    j1_rad = np.arctan2(c, d) - np.arctan2(a, b)

    # 关节3 腕部
    j3_deg = pitch_deg - np.rad2deg(j1_rad) - np.rad2deg(j2_rad)

    j0 = np.rad2deg(j0_rad)
    j1 = np.rad2deg(j1_rad)
    j2 = np.rad2deg(j2_rad)
    j3 = j3_deg

    # 关节限位检查
    angles = [j0, j1, j2, j3]
    for i, (lo, hi) in enumerate(JOINT_LIMITS):
        if not (lo <= angles[i] <= hi):
            return None

    return j0, j1, j2, j3


def pitch_range_solve(x, y, z, pitch, min_pitch, max_pitch):
    """
    与 arm_control.c 中 ArmControl_CoordinateSet 对应
    在 [min_pitch, max_pitch] 范围内尝试求解，返回关节角度或 None
    """
    for p in [pitch, min_pitch, max_pitch, (min_pitch + max_pitch) / 2]:
        result = kin_inverse(x, y, z, p)
        if result is not None:
            return result
    return None


# ── 手眼标定专用位置列表 ────────────────────────────────────────
# 格式：(x, y, z, pitch)  单位：cm / 度
# 与 hal_entry.c 中的标定序列 CALIB_POSES[] 完全一致
# 标定台（ArUco标记）放在机械臂正前方约25cm处的桌面上
CALIB_POSITIONS = [
    (20,   0,  20,   0),   # 位置 01
    (20,  -8,  20,   0),   # 位置 02
    (20,   8,  20,   0),   # 位置 03
    (20,  -8,  15, -15),   # 位置 04
    (20,   8,  15,  15),   # 位置 05
    (25,   0,  15,  -5),   # 位置 06
    (25,  -6,  15, -10),   # 位置 07
    (25,   6,  15,  10),   # 位置 08
    (15,   0,  20,   5),   # 位置 09
    (15,  -8,  18,  -5),   # 位置 10
    (15,   8,  18,   5),   # 位置 11
    (22,   0,  22,  10),   # 位置 12
    (22, -10,  18,   0),   # 位置 13
    (22,  10,  18,   0),   # 位置 14
    (18,   0,  15, -20),   # 位置 15
    (18,  -8,  22,  15),   # 位置 16
    (18,   8,  22,  15),   # 位置 17
    (23,  -8,  12, -15),   # 位置 18
]


if __name__ == "__main__":
    print("=" * 55)
    print("验证标定位置列表（检查逆运动学是否有解）")
    print("=" * 55)
    print(f"{'编号':>4}  {'x':>6} {'y':>6} {'z':>6} {'pitch':>6}  {'状态':>6}")
    print("-" * 55)
    valid = 0
    for i, (x, y, z, p) in enumerate(CALIB_POSITIONS, 1):
        result = pitch_range_solve(x, y, z, p, -90, 90)
        ok = result is not None
        if ok:
            valid += 1
        status = "OK" if ok else "无解"
        print(f"  {i:2d}   {x:6.1f} {y:6.1f} {z:6.1f} {p:6.1f}  {status}")
    print("-" * 55)
    print(f"共 {len(CALIB_POSITIONS)} 个位置，{valid} 个有解，{len(CALIB_POSITIONS)-valid} 个无解")
