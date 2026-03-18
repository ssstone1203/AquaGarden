"""
第三步：求解手眼标定

前提条件：
  · 已完成 step2（model/calibration/handeye_data.npz 存在）

输出文件：
  · model/calibration/T_cam2end.npy  ← 相机→末端执行器的4x4变换矩阵
  · model/calibration/T_cam2end.txt  ← 方便阅读的文本格式

运行后会打印：
  · 旋转矩阵 R（相机相对末端的姿态）
  · 平移向量 t（相机安装位置，单位 cm）
  · 欧拉角（便于理解相机的安装角度）
"""

import cv2
import numpy as np
import os

DATA_DIR  = os.path.join(os.path.dirname(__file__), "../../model/calibration")
DATA_FILE = os.path.join(DATA_DIR, "handeye_data.npz")
os.makedirs(DATA_DIR, exist_ok=True)

# ── 加载数据 ─────────────────────────────────────────────────────
if not os.path.exists(DATA_FILE):
    print(f"[错误] 找不到数据文件：{DATA_FILE}")
    print("       请先运行 step2_collect_data.py")
    exit(1)

d = np.load(DATA_FILE)
R_end2base_arr   = d["R_end2base"]
t_end2base_arr   = d["t_end2base"]
R_target2cam_arr = d["R_target2cam"]
t_target2cam_arr = d["t_target2cam"]

n = len(R_end2base_arr)
print(f"[OK] 已加载 {n} 组标定数据")

if n < 5:
    print("[错误] 数据组数不足（至少需要5组）")
    exit(1)

# ── 尝试多种求解算法取最优 ────────────────────────────────────────
METHODS = {
    "TSAI":     cv2.CALIB_HAND_EYE_TSAI,
    "HORAUD":   cv2.CALIB_HAND_EYE_HORAUD,
    "ANDREFF":  cv2.CALIB_HAND_EYE_ANDREFF,
    "DANIILIDIS": cv2.CALIB_HAND_EYE_DANIILIDIS,
}

def rotation_error(R):
    """旋转矩阵是否接近正交矩阵（越接近1越好）"""
    return np.linalg.norm(R.T @ R - np.eye(3))

best_method = None
best_R = None
best_t = None
best_err = float("inf")

print("\n对比各种求解方法：")
for name, method in METHODS.items():
    try:
        R_cam2end, t_cam2end = cv2.calibrateHandEye(
            R_end2base_arr, t_end2base_arr,
            R_target2cam_arr, t_target2cam_arr,
            method=method
        )
        err = rotation_error(R_cam2end)
        t_norm = np.linalg.norm(t_cam2end)
        print(f"  {name:<14}: 旋转正交误差={err:.2e}  平移量={t_norm:.2f} cm")
        if err < best_err:
            best_err   = err
            best_method = name
            best_R = R_cam2end
            best_t = t_cam2end
    except Exception as e:
        print(f"  {name:<14}: 求解失败 - {e}")

print(f"\n最优方法：{best_method}")

# ── 构建 4x4 变换矩阵 ───────────────────────────────────────────
T_cam2end = np.eye(4)
T_cam2end[:3, :3] = best_R
T_cam2end[:3,  3] = best_t.flatten()

# ── 计算欧拉角（ZYX，便于理解安装角度） ──────────────────────────
def rot2euler_zyx(R):
    """旋转矩阵转 ZYX 欧拉角（度）"""
    sy = np.sqrt(R[0, 0]**2 + R[1, 0]**2)
    if sy > 1e-6:
        roll  = np.rad2deg(np.arctan2( R[2, 1], R[2, 2]))
        pitch = np.rad2deg(np.arctan2(-R[2, 0], sy))
        yaw   = np.rad2deg(np.arctan2( R[1, 0], R[0, 0]))
    else:
        roll  = np.rad2deg(np.arctan2(-R[1, 2], R[1, 1]))
        pitch = np.rad2deg(np.arctan2(-R[2, 0], sy))
        yaw   = 0.0
    return roll, pitch, yaw

roll, pitch, yaw = rot2euler_zyx(best_R)
tx, ty, tz = best_t.flatten()

print("\n====== 手眼标定结果 ======")
print(f"\n旋转矩阵 R_cam2end：")
print(best_R)
print(f"\n平移向量 t_cam2end（cm）：")
print(f"  x = {tx:.3f} cm")
print(f"  y = {ty:.3f} cm")
print(f"  z = {tz:.3f} cm")
print(f"\n相机安装角度（欧拉角 ZYX）：")
print(f"  Roll  = {roll:.2f}°")
print(f"  Pitch = {pitch:.2f}°")
print(f"  Yaw   = {yaw:.2f}°")
print(f"\n相机离末端执行器距离：{np.linalg.norm(best_t):.2f} cm")

# ── 验证：计算重投影一致性 ────────────────────────────────────────
print("\n====== 一致性验证（越小越好）======")
errs = []
for i in range(n):
    T_e2b = np.eye(4)
    T_e2b[:3, :3] = R_end2base_arr[i]
    T_e2b[:3,  3] = t_end2base_arr[i].flatten()

    T_t2c = np.eye(4)
    T_t2c[:3, :3] = R_target2cam_arr[i]
    T_t2c[:3,  3] = t_target2cam_arr[i].flatten()

    # T_target_in_base = T_e2b @ T_cam2end @ T_t2c
    T_tib = T_e2b @ T_cam2end @ T_t2c
    errs.append(T_tib[:3, 3])

# 所有位置下标记在基座坐标中应相同（因为标记固定不动）
errs_arr = np.array(errs)
mean_pos = errs_arr.mean(axis=0)
std_pos  = errs_arr.std(axis=0)
print(f"标记在基座坐标系下的平均位置: [{mean_pos[0]:.2f}, {mean_pos[1]:.2f}, {mean_pos[2]:.2f}] cm")
print(f"各组偏差（标准差）: [{std_pos[0]:.2f}, {std_pos[1]:.2f}, {std_pos[2]:.2f}] cm")
print(f"总偏差（RMS）: {np.linalg.norm(std_pos):.3f} cm")

if np.linalg.norm(std_pos) < 0.5:
    print("✓ 标定精度很好（偏差 < 0.5 cm）")
elif np.linalg.norm(std_pos) < 1.0:
    print("△ 标定精度尚可（偏差 < 1 cm），可以接受")
else:
    print("✗ 标定精度较差，建议重新采集更多数据，或检查标记是否移动过")

# ── 保存结果 ─────────────────────────────────────────────────────
npy_path = os.path.join(DATA_DIR, "T_cam2end.npy")
txt_path = os.path.join(DATA_DIR, "T_cam2end.txt")

np.save(npy_path, T_cam2end)

with open(txt_path, "w", encoding="utf-8") as f:
    f.write("手眼标定结果：相机坐标系 → 末端执行器坐标系\n")
    f.write("位置单位：cm\n\n")
    f.write("4x4 变换矩阵 T_cam2end:\n")
    for row in T_cam2end:
        f.write("  " + "  ".join(f"{v:10.6f}" for v in row) + "\n")
    f.write(f"\n安装位置（cm）: x={tx:.3f}  y={ty:.3f}  z={tz:.3f}\n")
    f.write(f"安装角度（°）:  Roll={roll:.2f}  Pitch={pitch:.2f}  Yaw={yaw:.2f}\n")
    f.write(f"\n求解方法：{best_method}\n")
    f.write(f"旋转正交误差：{best_err:.2e}\n")

print(f"\n[OK] 结果已保存：")
print(f"     {npy_path}")
print(f"     {txt_path}")
print("\n手眼标定完成！后续使用时加载 T_cam2end.npy 即可。")
