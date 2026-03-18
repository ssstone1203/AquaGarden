"""
第二步：手眼标定数据采集

前提条件：
  · 已完成 step1（model/calibration/camera_intrinsics.npz 存在）
  · 机械臂固件已烧录"标定模式"（hal_entry.c 已修改为循环18个标定位置）
  · ArUco标记固定在桌面上（不能移动！）

操作步骤：
  1. 将 ArUco 标记贴在桌面，放在机械臂正前方约 25cm 处
  2. 量一下 ArUco 标记黑色方块的边长（cm），填入下方 MARKER_SIZE
  3. 给机械臂上电，固件会自动从位置1开始，每隔5秒切换一个位置
  4. 运行本脚本，脚本会实时显示摄像头画面
  5. 等机械臂停稳后，画面里能看到标记，按 [空格] 采集一组数据
  6. 重复以上步骤，依次采集 18 个位置（也可只采集能看到标记的位置）
  7. 按 [q] 退出并保存数据

输出文件：../../model/calibration/handeye_data.npz
"""

import cv2
import numpy as np
import os
import sys

# ── 参数配置 ────────────────────────────────────────────────────
MARKER_SIZE   = 4.0      # ArUco标记黑色方块的实际边长（cm）！！量好填对！！
CAMERA_INDEX  = 0        # 摄像头编号
OUTPUT_DIR    = os.path.join(os.path.dirname(__file__), "../../model/calibration")
INTRINSICS    = os.path.join(OUTPUT_DIR, "camera_intrinsics.npz")
# ─────────────────────────────────────────────────────────────────

sys.path.insert(0, os.path.dirname(__file__))
from kinematics import CALIB_POSITIONS, pitch_range_solve, kin_forward_matrix

os.makedirs(OUTPUT_DIR, exist_ok=True)

# ── 加载相机内参 ─────────────────────────────────────────────────
if not os.path.exists(INTRINSICS):
    print(f"[错误] 找不到内参文件：{INTRINSICS}")
    print("       请先运行 step1_camera_calib.py")
    exit(1)

data = np.load(INTRINSICS)
K    = data["K"]
dist = data["dist"]
print(f"[OK] 已加载相机内参，分辨率 {data['img_size']}")

# ── 初始化 ArUco 检测器 ──────────────────────────────────────────
ARUCO_DICTS = [
    ("DICT_4X4_50",  cv2.aruco.DICT_4X4_50),
    ("DICT_4X4_100", cv2.aruco.DICT_4X4_100),
    ("DICT_5X5_50",  cv2.aruco.DICT_5X5_50),
    ("DICT_6X6_50",  cv2.aruco.DICT_6X6_50),
]

def make_detector(dict_id):
    aruco_dict   = cv2.aruco.getPredefinedDictionary(dict_id)
    aruco_params = cv2.aruco.DetectorParameters()
    return cv2.aruco.ArucoDetector(aruco_dict, aruco_params)

def detect_marker(frame):
    """
    在所有常见字典中尝试检测 ArUco 标记
    返回 (corners, marker_id, dict_name) 或 None
    """
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    for name, dict_id in ARUCO_DICTS:
        detector = make_detector(dict_id)
        corners, ids, _ = detector.detectMarkers(gray)
        if ids is not None and len(ids) > 0:
            return corners[0], int(ids[0][0]), name
    return None

def estimate_pose(corners, K, dist, marker_size):
    """
    用 solvePnP 估算标记相对于相机的位姿
    返回 4x4 T_marker2cam 矩阵（位置单位：cm）
    """
    half = marker_size / 2.0
    obj_pts = np.array([
        [-half,  half, 0],
        [ half,  half, 0],
        [ half, -half, 0],
        [-half, -half, 0],
    ], dtype=np.float32)
    img_pts = corners.reshape(4, 2).astype(np.float32)

    ret, rvec, tvec = cv2.solvePnP(obj_pts, img_pts, K, dist)
    if not ret:
        return None

    R, _ = cv2.Rodrigues(rvec)
    T = np.eye(4)
    T[:3, :3] = R
    T[:3,  3] = tvec.flatten()
    return T


# ── 打开摄像头 ───────────────────────────────────────────────────
cap = cv2.VideoCapture(CAMERA_INDEX)
if not cap.isOpened():
    print(f"[错误] 无法打开摄像头 {CAMERA_INDEX}")
    exit(1)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

# 摄像头反向安装（旋转180°）
CAMERA_ROTATED_180 = True

# ── 数据存储 ─────────────────────────────────────────────────────
R_end2base_list  = []   # 末端相对基座的旋转矩阵列表
t_end2base_list  = []   # 末端相对基座的平移向量列表
R_target2cam_list = []  # 标记相对相机的旋转矩阵列表
t_target2cam_list = []  # 标记相对相机的平移向量列表
captured_poses    = []  # 记录采集了哪些位置

current_pos_idx = 0     # 当前准备采集的位置编号（0-based）

print("\n" + "=" * 60)
print("手眼标定数据采集  ——  操作说明")
print("=" * 60)
print(f"  共 {len(CALIB_POSITIONS)} 个标定位置")
print("  · 机械臂已在固件中自动循环这些位置（每5秒切换一次）")
print("  · 等机械臂停稳，画面中看到标记后，按 [空格] 采集")
print("  · 按 [n] 跳过当前位置（标记不可见时）")
print("  · 采集完所有位置后按 [q] 保存退出")
print("  · 至少需要采集 10 组才能计算（建议15组以上）")
print("=" * 60)

while True:
    ret, frame = cap.read()
    if not ret:
        break

    if CAMERA_ROTATED_180:
        frame = cv2.rotate(frame, cv2.ROTATE_180)

    display = frame.copy()
    result  = detect_marker(frame)

    # 标注当前目标位置信息
    if current_pos_idx < len(CALIB_POSITIONS):
        x, y, z, p = CALIB_POSITIONS[current_pos_idx]
        pos_text = f"目标位置 {current_pos_idx+1}/{len(CALIB_POSITIONS)}: " \
                   f"({x:.0f}, {y:.0f}, {z:.0f}) pitch={p:.0f}°"
    else:
        pos_text = f"所有位置已采集！按 q 保存退出"

    cv2.putText(display, pos_text, (10, 35),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
    cv2.putText(display, f"已采集: {len(R_end2base_list)} 组", (10, 65),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

    if result is not None:
        corners, marker_id, dict_name = result
        T_target2cam = estimate_pose(corners, K, dist, MARKER_SIZE)

        # 绘制标记边框
        cv2.aruco.drawDetectedMarkers(display, [corners])
        if T_target2cam is not None:
            cv2.drawFrameAxes(display, K, dist,
                              cv2.Rodrigues(T_target2cam[:3, :3])[0],
                              T_target2cam[:3, 3], MARKER_SIZE * 0.5)
            dist_cm = np.linalg.norm(T_target2cam[:3, 3])
            cv2.putText(display,
                        f"标记ID={marker_id}  字典={dict_name}  距离={dist_cm:.1f}cm",
                        (10, 95), cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 200, 255), 2)
        cv2.putText(display, "[空格] 采集  [n] 跳过  [q] 退出",
                    (10, display.shape[0] - 15),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.65, (200, 200, 200), 2)
    else:
        cv2.putText(display, "未检测到 ArUco 标记，调整位置或光线",
                    (10, 95), cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 0, 255), 2)

    cv2.imshow("手眼标定数据采集 - step2", display)
    key = cv2.waitKey(1) & 0xFF

    if key == ord('q'):
        break

    elif key == ord('n'):
        # 跳过当前位置
        print(f"  跳过位置 {current_pos_idx + 1}")
        current_pos_idx += 1

    elif key == ord(' ') and result is not None:
        corners, marker_id, dict_name = result
        T_target2cam = estimate_pose(corners, K, dist, MARKER_SIZE)

        if T_target2cam is None:
            print("  [警告] 位姿估算失败，请重试")
            continue

        if current_pos_idx >= len(CALIB_POSITIONS):
            print("  [提示] 所有位置已采集，按 q 退出")
            continue

        # 计算末端位姿（从已知坐标→IK→FK→T_end2base）
        x, y, z, p = CALIB_POSITIONS[current_pos_idx]
        joints = pitch_range_solve(x, y, z, p, -90, 90)
        if joints is None:
            print(f"  [错误] 位置{current_pos_idx+1}逆运动学无解，跳过")
            current_pos_idx += 1
            continue

        T_end2base = kin_forward_matrix(*joints)

        # 保存数据
        R_end2base_list.append(T_end2base[:3, :3])
        t_end2base_list.append(T_end2base[:3,  3].reshape(3, 1))
        R_target2cam_list.append(T_target2cam[:3, :3])
        t_target2cam_list.append(T_target2cam[:3,  3].reshape(3, 1))
        captured_poses.append(current_pos_idx + 1)

        print(f"  ✓ 采集位置 {current_pos_idx+1}  关节角={[f'{j:.1f}' for j in joints]}")
        current_pos_idx += 1

        # 保存闪烁提示
        cv2.putText(display, "已保存!", (display.shape[1]//2 - 80, display.shape[0]//2),
                    cv2.FONT_HERSHEY_SIMPLEX, 2, (0, 255, 0), 4)
        cv2.imshow("手眼标定数据采集 - step2", display)
        cv2.waitKey(600)

cap.release()
cv2.destroyAllWindows()

n = len(R_end2base_list)
print(f"\n共采集 {n} 组数据，采集的位置编号：{captured_poses}")

if n < 5:
    print("[错误] 数据太少（至少需要5组），请重新采集")
    exit(1)
elif n < 10:
    print("[警告] 数据偏少，建议采集15组以上以获得更好的精度")

# 保存数据
out_path = os.path.join(OUTPUT_DIR, "handeye_data.npz")
np.savez(out_path,
         R_end2base   = np.array(R_end2base_list),
         t_end2base   = np.array(t_end2base_list),
         R_target2cam = np.array(R_target2cam_list),
         t_target2cam = np.array(t_target2cam_list),
         captured_poses = np.array(captured_poses))
print(f"[OK] 数据已保存到：{out_path}")
print("     下一步：运行 step3_solve_handeye.py")
