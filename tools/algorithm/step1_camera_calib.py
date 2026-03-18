"""
第一步：相机内参标定

你需要一张棋盘格图案（从网上打印，粘在硬纸板上）。
推荐尺寸：A4纸，9x6内角点棋盘格，每格约25mm

操作步骤：
  1. 把摄像头用USB接到电脑
  2. 运行本脚本
  3. 手持棋盘格，在摄像头前慢慢移动、倾斜、旋转
  4. 当看到棋盘格角点被标出时，按 空格键 保存一帧
  5. 多角度拍摄至少 20 张（越多越准）
  6. 按 q 结束，自动计算并保存内参

输出文件：../../model/calibration/camera_intrinsics.npz
"""

import cv2
import numpy as np
import os

# ── 参数配置（根据你打印的棋盘格修改）────────────────────────
BOARD_COLS   = 9        # 棋盘格横向内角点数
BOARD_ROWS   = 6        # 棋盘格纵向内角点数
SQUARE_SIZE  = 2.5      # 每格边长（cm）
CAMERA_INDEX = 0        # 摄像头编号（一般为0）
MIN_IMAGES   = 20       # 最少采集帧数
OUTPUT_DIR   = os.path.join(os.path.dirname(__file__), "../../model/calibration")
# ─────────────────────────────────────────────────────────────

os.makedirs(OUTPUT_DIR, exist_ok=True)

criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
board_size = (BOARD_COLS, BOARD_ROWS)

# 棋盘格世界坐标（z=0 的平面）
objp = np.zeros((BOARD_COLS * BOARD_ROWS, 3), np.float32)
objp[:, :2] = np.mgrid[0:BOARD_COLS, 0:BOARD_ROWS].T.reshape(-1, 2) * SQUARE_SIZE

obj_points = []   # 世界坐标
img_points = []   # 图像坐标

cap = cv2.VideoCapture(CAMERA_INDEX)
if not cap.isOpened():
    print(f"[错误] 无法打开摄像头 {CAMERA_INDEX}，请检查连接")
    exit(1)

cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

# 摄像头反向安装（旋转180°）
CAMERA_ROTATED_180 = True

print("=" * 55)
print("相机内参标定  ——  操作说明")
print("=" * 55)
print("  · 手持棋盘格在摄像头前缓慢移动")
print("  · 看到绿色角点时按 [空格] 保存")
print(f"  · 至少采集 {MIN_IMAGES} 张后按 [q] 计算内参")
print("=" * 55)

n_captured = 0

while True:
    ret, frame = cap.read()
    if not ret:
        print("[错误] 读取摄像头失败")
        break

    if CAMERA_ROTATED_180:
        frame = cv2.rotate(frame, cv2.ROTATE_180)

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    found, corners = cv2.findChessboardCorners(gray, board_size, None)

    display = frame.copy()
    if found:
        corners_sub = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
        cv2.drawChessboardCorners(display, board_size, corners_sub, found)
        cv2.putText(display, "检测到棋盘格！按空格保存", (10, 35),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 0), 2)
    else:
        cv2.putText(display, "未检测到棋盘格", (10, 35),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 0, 255), 2)

    cv2.putText(display, f"已采集: {n_captured} / {MIN_IMAGES}  按q结束", (10, 70),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
    cv2.imshow("相机内参标定 - step1", display)

    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break
    elif key == ord(' ') and found:
        corners_sub = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
        obj_points.append(objp)
        img_points.append(corners_sub)
        n_captured += 1
        print(f"  保存第 {n_captured} 张")
        # 闪烁提示
        cv2.putText(display, "已保存!", (display.shape[1]//2 - 60, display.shape[0]//2),
                    cv2.FONT_HERSHEY_SIMPLEX, 2, (0, 255, 255), 4)
        cv2.imshow("相机内参标定 - step1", display)
        cv2.waitKey(500)

cap.release()
cv2.destroyAllWindows()

if n_captured < 5:
    print(f"[警告] 只采集了 {n_captured} 张，不足以标定，请重新运行")
    exit(1)

print(f"\n开始计算内参（共 {n_captured} 张）...")
h, w = gray.shape
ret, K, dist, rvecs, tvecs = cv2.calibrateCamera(
    obj_points, img_points, (w, h), None, None
)

# 计算重投影误差
total_err = 0.0
for i in range(len(obj_points)):
    projected, _ = cv2.projectPoints(obj_points[i], rvecs[i], tvecs[i], K, dist)
    err = cv2.norm(img_points[i], projected, cv2.NORM_L2) / len(projected)
    total_err += err
mean_err = total_err / len(obj_points)

print("\n====== 标定结果 ======")
print(f"相机内参矩阵 K:\n{K}")
print(f"\n畸变系数 dist:\n{dist}")
print(f"\n重投影误差（越小越好）: {mean_err:.4f} px")
if mean_err > 1.0:
    print("[提示] 误差较大，建议重新采集更多、更好的图片")
else:
    print("[OK] 误差在正常范围内")

# 保存
out_path = os.path.join(OUTPUT_DIR, "camera_intrinsics.npz")
np.savez(out_path, K=K, dist=dist, img_size=np.array([w, h]))
print(f"\n内参已保存到：{out_path}")
