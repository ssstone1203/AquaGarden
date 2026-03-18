"""
ArUco 检测诊断工具
  · 加载相机内参 → 去畸变 → CLAHE 增强 → 多字典检测
  · 按 [u] 切换是否显示去畸变画面
  · 按 [q] 退出
"""

import cv2
import cv2.aruco as aruco
import numpy as np
import os
from PIL import ImageFont, ImageDraw, Image

CAMERA_INDEX  = 1
MARKER_SIZE   = 2.5   # cm

INTRINSICS = os.path.join(os.path.dirname(__file__),
                           "../../model/calibration/camera_intrinsics.npz")

# ── 加载内参 ──────────────────────────────────────────────────────
K_cam = dist_cam = None
if os.path.exists(INTRINSICS):
    raw      = np.load(INTRINSICS)
    K_cam    = raw["K"]
    dist_cam = raw["dist"]
    print(f"[OK] 已加载内参：{INTRINSICS}")
else:
    print("[警告] 未找到内参文件，去畸变功能不可用")

# ── 检测器（宽松参数）────────────────────────────────────────────
def _make_apriltag_detector():
    d = aruco.getPredefinedDictionary(aruco.DICT_APRILTAG_36h11)
    p = aruco.DetectorParameters()
    p.adaptiveThreshWinSizeMin    = 3
    p.adaptiveThreshWinSizeMax    = 23
    p.adaptiveThreshWinSizeStep   = 10
    p.polygonalApproxAccuracyRate = 0.05
    p.minMarkerPerimeterRate      = 0.02
    p.cornerRefinementMethod      = aruco.CORNER_REFINE_SUBPIX
    return aruco.ArucoDetector(d, p)

DETECTOR = _make_apriltag_detector()
CLAHE     = cv2.createCLAHE(clipLimit=2.5, tileGridSize=(8, 8))

# ── 中文字体 ──────────────────────────────────────────────────────
def _load_font(size=22):
    for p in ["C:/Windows/Fonts/msyh.ttc",
              "C:/Windows/Fonts/simhei.ttf",
              "C:/Windows/Fonts/simsun.ttc"]:
        try:    return ImageFont.truetype(p, size)
        except: pass
    return ImageFont.load_default()

_font22 = _load_font(22)
_font26 = _load_font(26)

def put_cn(img, text, pos, color=(0, 255, 0), font=None):
    pil = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
    ImageDraw.Draw(pil).text(pos, text, font=font or _font22,
                             fill=(color[2], color[1], color[0]))
    return cv2.cvtColor(np.array(pil), cv2.COLOR_RGB2BGR)

# ── 检测函数 ──────────────────────────────────────────────────────
def detect_best(frame):
    """返回 (corners, ids, dict_name) 或 (None, None, '')"""
    gray    = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    gray_eq = CLAHE.apply(gray)

    grays = [gray_eq, gray]
    if K_cam is not None:
        grays.insert(0, cv2.undistort(gray_eq, K_cam, dist_cam))

    for g in grays:
        corners, ids, _ = DETECTOR.detectMarkers(g)
        if ids is not None and len(ids) > 0:
            return corners, ids, "AprilTag_36h11"
    return None, None, ""

# ── 摄像头 ────────────────────────────────────────────────────────
cap = cv2.VideoCapture(CAMERA_INDEX)
if not cap.isOpened():
    print(f"无法打开摄像头 {CAMERA_INDEX}，尝试 0")
    cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

show_undist = False   # 按 [u] 切换
print("按 [u] 切换去畸变视图  |  按 [q] 退出")

while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.rotate(frame, cv2.ROTATE_180)   # 摄像头倒装

    # 去畸变画面（用于显示）
    undist = cv2.undistort(frame, K_cam, dist_cam) if K_cam is not None else frame
    display = undist if show_undist else frame

    corners, ids, dict_name = detect_best(frame)

    if ids is not None:
        aruco.drawDetectedMarkers(display, corners, ids)
        for i, corner in enumerate(corners):
            pts = corner[0].astype(int)
            cx  = int(pts[:, 0].mean())
            cy  = int(pts[:, 1].mean())
            mid = int(np.max(np.linalg.norm(pts - [cx, cy], axis=1)) * 1.3) + 5
            cv2.circle(display, (cx, cy), mid, (0, 220, 0), 3)
            cv2.polylines(display, [pts], True, (0, 220, 0), 2)
            label = f"ID:{ids[i][0]}  [{dict_name}]"
            display = put_cn(display, label, (cx - 60, cy - mid - 30),
                             color=(0, 220, 0), font=_font22)

            if K_cam is not None:
                half    = MARKER_SIZE / 2.0
                obj_pts = np.array([[-half,  half, 0], [ half,  half, 0],
                                    [ half, -half, 0], [-half, -half, 0]],
                                   dtype=np.float32)
                ok, rvec, tvec = cv2.solvePnP(obj_pts,
                                               corner.reshape(4,2).astype(np.float32),
                                               K_cam, dist_cam)
                if ok:
                    d_cm = float(np.linalg.norm(tvec))
                    cv2.putText(display, f"{d_cm:.1f}cm",
                                (cx + mid + 6, cy + 6),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 220, 200), 2)

        status = f"检测到 {len(ids)} 个码  字典:{dict_name}"
        color  = (0, 220, 0)
    else:
        status = "未检测到 ArUco 码"
        color  = (50, 50, 255)

    display = put_cn(display, status, (10, 6), color=color, font=_font26)
    view_label = "去畸变" if show_undist else "原始画面"
    display = put_cn(display, f"[u] {view_label}  [q] 退出",
                     (10, display.shape[0] - 36),
                     color=(160, 160, 160), font=_font22)

    cv2.imshow("ArUco 诊断", display)
    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break
    if key == ord('u'):
        show_undist = not show_undist
        print(f"  → {'去畸变' if show_undist else '原始'}画面")

cap.release()
cv2.destroyAllWindows()
