"""
第二步：手眼标定数据采集（串口控制版）

前提条件：
  · 已完成 step1（model/calibration/camera_intrinsics.npz 存在）
  · AprilTag 36h11标记固定在桌面上，标定过程中不能移动！
  · 固件已烧录 PC_CONTROL 模式（global.h 中 CONTROL_MODE = 0）
  · 机械臂通过 USB 串口连接到电脑

工作原理：
  · Python 启动后发送 "CALIB_RESET\\n"，机械臂归位到位置 1（ArUco 正中心）
  · 用户等机械臂停稳、看到绿圈后按 [空格] 采集，Python 自动发 "1\\n"
  · 机械臂移动到下一个位置，移动完成后固件回复 "OK\\n"
  · 重复以上步骤直到所有 21 个位置采集完毕

操作键：
  [空格]  采集当前位置数据并驱动机械臂移到下一个位置
  [n]     跳过当前位置（不采集，但仍驱动机械臂移到下一个位置）
  [q]     保存退出

输出文件：../../model/calibration/handeye_data.npz

依赖：pip install pyserial
"""

import cv2
import numpy as np
import os
import sys
import time
from PIL import Image, ImageDraw, ImageFont

try:
    import serial
    _SERIAL_AVAILABLE = True
except ImportError:
    _SERIAL_AVAILABLE = False
    print("[警告] 未安装 pyserial，以仿真模式运行（不驱动机械臂）")
    print("       安装方法：pip install pyserial")

# ── 串口配置 ──────────────────────────────────────────────────────
SERIAL_PORT = "COM5"      # 机械臂串口，根据设备管理器修改
SERIAL_BAUD = 115200
USE_SERIAL  = True        # False = 仿真模式（只采集视觉数据）

# ── 摄像头 / 标记配置 ─────────────────────────────────────────────
MARKER_SIZE        = 2.5  # ArUco 标记实际边长（cm）
CAMERA_INDEX       = 1
CAMERA_ROTATED_180 = True

# ── 路径 ──────────────────────────────────────────────────────────
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../model/calibration")
INTRINSICS = os.path.join(OUTPUT_DIR, "camera_intrinsics.npz")
# ──────────────────────────────────────────────────────────────────

sys.path.insert(0, os.path.dirname(__file__))
from kinematics import pitch_range_solve, kin_forward_matrix

os.makedirs(OUTPUT_DIR, exist_ok=True)

# ── 手眼标定位姿表（与 hardware/arm/src/hal_entry.c 中 s_calib_poses[] 完全一致）──
# 格式：(x, y, z, pitch)  单位：cm / 度
CALIB_POSES = [
    (13.0,  0.0, -3.0, -68.0),  # 01  中轴近距低pitch
    (13.0,  0.0, -6.5, -83.0),  # 02  中轴近距高pitch
    (16.0,  0.0, -2.0, -62.0),  # 03  中轴中距低pitch
    (16.0,  0.0, -4.5, -76.0),  # 04  中轴中距中pitch（接近原始中心）
    (16.0,  0.0, -7.5, -88.0),  # 05  中轴中距高pitch
    (19.0,  0.0, -3.0, -64.0),  # 06  中轴远距低pitch
    (19.0,  0.0, -6.0, -78.0),  # 07  中轴远距中pitch
    (13.0, -5.0, -3.5, -72.0),  # 08  右侧近距低pitch（j0≈-21°）
    (13.0, -5.0, -6.5, -83.0),  # 09  右侧近距高pitch
    (16.0, -6.0, -3.0, -68.0),  # 10  右侧中距低pitch（j0≈-21°）
    (16.0, -6.0, -6.0, -80.0),  # 11  右侧中距高pitch
    (16.0, -8.0, -4.0, -76.0),  # 12  右侧大偏角（j0≈-27°）
    (13.0,  5.0, -3.5, -72.0),  # 13  左侧近距低pitch（j0≈+21°）
    (13.0,  5.0, -6.5, -83.0),  # 14  左侧近距高pitch
    (16.0,  6.0, -3.0, -68.0),  # 15  左侧中距低pitch（j0≈+21°）
    (16.0,  6.0, -6.0, -80.0),  # 16  左侧中距高pitch
    (16.0,  8.0, -4.0, -76.0),  # 17  左侧大偏角（j0≈+27°）
    (15.0,  0.0, -8.5, -88.0),  # 18  极限低位
    (18.0,  0.0, -1.5, -58.0),  # 19  极限高位低pitch
    (18.0,  0.0, -9.0, -88.0),  # 20  极限高位高pitch
    (12.0,  0.0, -3.5, -78.0),  # 21  最近距离
    (12.0, -4.0, -5.0, -80.0),  # 22  最近距离右偏
    (12.0,  4.0, -5.0, -80.0),  # 23  最近距离左偏
]
N_POSES = len(CALIB_POSES)


# ── 中文字体 ──────────────────────────────────────────────────────
def _find_chinese_font() -> str:
    for p in [
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/System/Library/Fonts/PingFang.ttc",
    ]:
        if os.path.exists(p):
            return p
    return ""

_FONT_PATH = _find_chinese_font()


def put_chinese_text(img: np.ndarray, text: str, pos: tuple,
                     font_size: int = 26,
                     color: tuple = (255, 255, 255)) -> np.ndarray:
    """在 OpenCV 图像上渲染中文（BGR 输入/输出）"""
    img_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    pil_img = Image.fromarray(img_rgb)
    draw    = ImageDraw.Draw(pil_img)
    font    = (ImageFont.truetype(_FONT_PATH, font_size)
               if _FONT_PATH else ImageFont.load_default())
    draw.text(pos, text, font=font, fill=(color[2], color[1], color[0]))
    return cv2.cvtColor(np.array(pil_img), cv2.COLOR_RGB2BGR)


# ── AprilTag 36h11 检测 ───────────────────────────────────────────
def _make_apriltag_detector() -> cv2.aruco.ArucoDetector:
    """AprilTag 36h11 检测器（与成功案例参数完全一致）"""
    d = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_APRILTAG_36h11)
    p = cv2.aruco.DetectorParameters()
    p.adaptiveThreshWinSizeMin    = 3
    p.adaptiveThreshWinSizeMax    = 23
    p.adaptiveThreshWinSizeStep   = 10
    p.polygonalApproxAccuracyRate = 0.05
    p.minMarkerPerimeterRate      = 0.02
    p.cornerRefinementMethod      = cv2.aruco.CORNER_REFINE_SUBPIX
    return cv2.aruco.ArucoDetector(d, p)

_DETECTOR = _make_apriltag_detector()

# CLAHE 增强对比度（应对光照不均）
_CLAHE = cv2.createCLAHE(clipLimit=2.5, tileGridSize=(8, 8))


def detect_marker(frame: np.ndarray,
                  cam_K=None, cam_dist=None):
    """
    AprilTag 36h11 检测。
    检测顺序（优先使用去畸变图，消除鱼眼/桶形畸变影响）：
      1. 去畸变 + CLAHE 灰度（若提供 cam_K/cam_dist）
      2. 原始 CLAHE 灰度
      3. 原始灰度（兜底）
    返回 (corners_4x2, marker_id) 或 None
    """
    gray    = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    gray_eq = _CLAHE.apply(gray)

    candidates = [gray_eq, gray]
    if cam_K is not None and cam_dist is not None:
        gray_undist = cv2.undistort(gray_eq, cam_K, cam_dist)
        candidates  = [gray_undist, gray_eq, gray]

    for gray_try in candidates:
        corners, ids, _ = _DETECTOR.detectMarkers(gray_try)
        if ids is not None and len(ids) > 0:
            return corners[0], int(ids[0][0])
    return None


def estimate_pose(corners: np.ndarray, K: np.ndarray,
                  dist: np.ndarray, marker_size: float):
    """solvePnP 估算标记→相机的 4×4 变换矩阵（cm）"""
    half    = marker_size / 2.0
    obj_pts = np.array([[-half,  half, 0],
                        [ half,  half, 0],
                        [ half, -half, 0],
                        [-half, -half, 0]], dtype=np.float32)
    img_pts = corners.reshape(4, 2).astype(np.float32)
    ok, rvec, tvec = cv2.solvePnP(obj_pts, img_pts, K, dist)
    if not ok:
        return None
    R, _ = cv2.Rodrigues(rvec)
    T = np.eye(4)
    T[:3, :3] = R
    T[:3,  3] = tvec.flatten()
    return T


def draw_marker_highlight(img: np.ndarray, corners: np.ndarray,
                          color=(0, 230, 0), thickness: int = 3):
    """在 ArUco 码周围绘制绿色圆圈 + 边框，返回 (center_xy, radius)"""
    pts    = corners.reshape(4, 2)
    center = pts.mean(axis=0).astype(int)
    radius = int(np.max(np.linalg.norm(pts - center, axis=1)) * 1.4) + 6
    cv2.circle(img, tuple(center), radius, color, thickness)
    cv2.polylines(img, [pts.astype(int)],
                  isClosed=True, color=color, thickness=2)
    return tuple(center), radius


# ── 串口通信 ──────────────────────────────────────────────────────
ser = None
_ser_buf = b""


def serial_connect() -> bool:
    """尝试打开串口，返回是否成功"""
    global ser
    if not _SERIAL_AVAILABLE or not USE_SERIAL:
        return False
    try:
        ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=0)
        time.sleep(0.3)
        ser.reset_input_buffer()
        print(f"[OK] 串口已打开：{SERIAL_PORT}  {SERIAL_BAUD}bps")
        return True
    except Exception as e:
        print(f"[警告] 无法打开串口 {SERIAL_PORT}：{e}")
        print("       以仿真模式运行（只采集视觉数据，不驱动机械臂）")
        ser = None
        return False


def serial_send(cmd: str) -> None:
    """向机械臂发送一行命令（自动追加 \\n）"""
    if ser is None:
        return
    try:
        ser.write((cmd + "\n").encode())
    except Exception as e:
        print(f"[串口错误] 发送失败：{e}")


def serial_poll_line() -> str:
    """非阻塞轮询，若有完整一行则返回，否则返回空串"""
    global _ser_buf
    if ser is None:
        return ""
    try:
        if ser.in_waiting > 0:
            _ser_buf += ser.read(ser.in_waiting)
        if b"\n" in _ser_buf:
            parts    = _ser_buf.split(b"\n", 1)
            line     = parts[0].decode(errors="ignore").strip()
            _ser_buf = parts[1]
            return line
    except Exception:
        pass
    return ""


# ── 加载内参 ──────────────────────────────────────────────────────
if not os.path.exists(INTRINSICS):
    print(f"[错误] 找不到内参文件：{INTRINSICS}")
    print("       请先运行 step1_camera_calib.py")
    exit(1)

raw  = np.load(INTRINSICS)
K    = raw["K"]
dist = raw["dist"]
print(f"[OK] 已加载相机内参，分辨率 {raw['img_size']}")

# 打印位姿清单
print(f"\n标定位置清单（共 {N_POSES} 个）：")
print(f"{'编号':>4}  {'x':>6} {'y':>6} {'z':>6} {'pitch':>7}")
print("-" * 40)
for i, (x, y, z, p) in enumerate(CALIB_POSES):
    print(f"  {i+1:2d}   {x:6.1f} {y:6.1f} {z:6.1f} {p:7.1f}")
print("-" * 40)

# ── 打开摄像头 ────────────────────────────────────────────────────
cap = cv2.VideoCapture(CAMERA_INDEX)
if not cap.isOpened():
    print(f"[错误] 无法打开摄像头 {CAMERA_INDEX}")
    exit(1)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

# ── 连接串口 ──────────────────────────────────────────────────────
serial_connected = serial_connect()

# ── 数据存储 ──────────────────────────────────────────────────────
R_end2base_list   = []
t_end2base_list   = []
R_target2cam_list = []
t_target2cam_list = []
captured_poses    = []

# ── 状态机 ────────────────────────────────────────────────────────
# arm_moving    = True  → 正在等待机械臂完成运动（等串口 OK/DONE）
# arm_moving    = False → 机械臂就位，等待用户采集
# current_joints        → 固件回传的实际关节角 [j0,j1,j2,j3]（度）
#                         None 表示本次未获取到，退回到运动学解
current_idx    = 0
arm_moving     = False
all_done       = False
current_joints = None   # float[4] 或 None

print("\n" + "=" * 65)
print("手眼标定数据采集  ——  操作说明")
print("=" * 65)
if serial_connected:
    print(f"  串口：{SERIAL_PORT}  正在初始化，机械臂移动到位置 1...")
else:
    print("  仿真模式：机械臂不会移动，采集视觉数据时需手动定位")
print("  · 等机械臂停稳，画面出现带绿圈的 ArUco 码后，按 [空格] 采集")
print("  · 按 [n] 跳过当前位置，按 [q] 保存退出")
print(f"  · 建议采集全部 {N_POSES} 个位置（至少 10 个）")
print("=" * 65 + "\n")

# 初始化：发 CALIB_RESET，机械臂归位到位置 01
if serial_connected:
    serial_send("CALIB_RESET")
    arm_moving = True
    print(f"  → 发送 CALIB_RESET，机械臂移动到位置 1（中心位置）...")
else:
    arm_moving = False  # 仿真模式直接就绪


# ── 主循环 ────────────────────────────────────────────────────────
while True:
    ret, frame = cap.read()
    if not ret:
        print("[错误] 读取摄像头失败")
        break

    if CAMERA_ROTATED_180:
        frame = cv2.rotate(frame, cv2.ROTATE_180)

    h, w = frame.shape[:2]
    display = frame.copy()

    # ── 串口轮询（非阻塞）──────────────────────────────
    if arm_moving:
        resp = serial_poll_line()
        if resp.startswith("OK"):
            arm_moving     = False
            current_joints = None          # 先清空
            # 解析回传关节角：格式 "OK j0.dd,j1.dd,j2.dd,j3.dd"
            parts = resp.split(None, 1)    # ["OK", "j0,..."] 或 ["OK"]
            if len(parts) == 2:
                try:
                    vals = [float(v) for v in parts[1].split(',')]
                    if len(vals) == 4:
                        current_joints = vals
                        print(f"  → 机械臂就位，实际关节角="
                              f"[{', '.join(f'{v:.2f}' for v in vals)}] deg")
                except ValueError:
                    pass
            if current_joints is None:
                print(f"  → 机械臂就位（未收到关节角，将使用运动学解）")
        elif resp == "ERR":
            arm_moving     = False
            current_joints = None
            print(f"  [警告] 机械臂运动错误（位置 {current_idx + 1}），请检查")
        elif resp == "DONE":
            arm_moving     = False
            all_done       = True
            current_idx    = N_POSES
            current_joints = None
            print(f"  → 所有位置已完成（固件确认 DONE）")

    # ── ArUco 检测（仅在就位时检测）────────────────────
    result       = None
    T_target2cam = None
    if not arm_moving:
        result = detect_marker(frame, cam_K=K, cam_dist=dist)
        if result is not None:
            corners, marker_id = result
            T_target2cam = estimate_pose(corners, K, dist, MARKER_SIZE)
            center_px, radius = draw_marker_highlight(display, corners)
            if T_target2cam is not None:
                cv2.drawFrameAxes(display, K, dist,
                                  cv2.Rodrigues(T_target2cam[:3, :3])[0],
                                  T_target2cam[:3, 3],
                                  MARKER_SIZE * 0.6)
                dist_cm = np.linalg.norm(T_target2cam[:3, 3])
                cv2.putText(display, f"{dist_cm:.1f}cm",
                            (center_px[0] + radius + 8, center_px[1] + 6),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 230, 200), 2)

    # ── 顶部状态栏 ──────────────────────────────────────
    overlay = display.copy()
    cv2.rectangle(overlay, (0, 0), (w, 132), (0, 0, 0), -1)
    display = cv2.addWeighted(overlay, 0.55, display, 0.45, 0)

    if arm_moving:
        # 机械臂运动中
        line1 = (f"机械臂移动中...  位置 {current_idx + 1}/{N_POSES}"
                 f"   已采集 {len(R_end2base_list)} 组")
        display = put_chinese_text(display, line1, (10, 6),
                                   font_size=26, color=(255, 160, 0))
        display = put_chinese_text(display, "等待机械臂停稳...",
                                   (10, 46), font_size=24, color=(180, 180, 0))
    elif all_done or current_idx >= N_POSES:
        line1 = f"全部 {N_POSES} 个位置已完成！共采集 {len(R_end2base_list)} 组  按 q 保存退出"
        display = put_chinese_text(display, line1, (10, 6),
                                   font_size=26, color=(0, 255, 120))
    else:
        x, y, z, p = CALIB_POSES[current_idx]
        line1 = (f"位置 {current_idx + 1}/{N_POSES}  "
                 f"x={x:.1f}  y={y:.1f}  z={z:.1f}  pitch={p:.1f}°  "
                 f"已采集 {len(R_end2base_list)} 组")
        display = put_chinese_text(display, line1, (10, 6),
                                   font_size=26, color=(255, 220, 0))
        if result is not None:
            display = put_chinese_text(
                display, "检测到 ArUco 码  —  按空格采集",
                (10, 46), font_size=24, color=(0, 230, 0))
        else:
            display = put_chinese_text(
                display, "未检测到 ArUco 码，调整机械臂位置或光线",
                (10, 46), font_size=24, color=(60, 60, 255))

    # 串口状态指示（用 PIL 渲染，避免中文乱码）
    serial_label = f"串口: {SERIAL_PORT}" if serial_connected else "串口: 仿真模式"
    serial_color = (100, 200, 100) if serial_connected else (120, 120, 120)
    display = put_chinese_text(display, serial_label,
                               (w - 210, 6), font_size=20, color=serial_color)

    # ── 底部提示栏 ──────────────────────────────────────
    hint_y = h - 36
    hint_ov = display.copy()
    cv2.rectangle(hint_ov, (0, hint_y - 6), (w, h), (0, 0, 0), -1)
    display = cv2.addWeighted(hint_ov, 0.5, display, 0.5, 0)
    display = put_chinese_text(
        display, "空格 = 采集并移下一位    n = 跳过并移下一位    q = 保存退出",
        (10, hint_y), font_size=22, color=(180, 180, 180))

    cv2.imshow("手眼标定数据采集 - step2", display)
    key = cv2.waitKey(1) & 0xFF

    if key == ord('q'):
        break

    # 机械臂运动期间禁止操作
    if arm_moving:
        continue

    if key == ord('n') and current_idx < N_POSES:
        print(f"  跳过位置 {current_idx + 1}")
        current_idx    += 1
        current_joints  = None   # 清空
        if current_idx < N_POSES:
            serial_send("1")
            arm_moving = True
            if serial_connected:
                print(f"  → 发送 1，机械臂移动到位置 {current_idx + 1}...")

    elif key == ord(' ') and result is not None and current_idx < N_POSES:
        if T_target2cam is None:
            corners, _ = result
            T_target2cam = estimate_pose(corners, K, dist, MARKER_SIZE)
        if T_target2cam is None:
            print("  [警告] 位姿估算失败，请重试")
            continue

        # ── 计算 T_end2base ──────────────────────────────────
        # 优先使用固件回传的实际关节角（消除机械误差）；
        # 若未收到，则退回到目标坐标的运动学解。
        if current_joints is not None:
            joints     = tuple(current_joints)
            joints_src = "实际回传"
        else:
            x, y, z, p = CALIB_POSES[current_idx]
            joints = pitch_range_solve(x, y, z, p, -90, 90)
            if joints is None:
                print(f"  [错误] 位置 {current_idx + 1} 逆运动学无解，跳过")
                current_idx += 1
                if current_idx < N_POSES:
                    serial_send("1")
                    arm_moving     = True
                    current_joints = None
                continue
            joints_src = "运动学解"

        T_end2base = kin_forward_matrix(*joints)

        R_end2base_list.append(T_end2base[:3, :3])
        t_end2base_list.append(T_end2base[:3,  3].reshape(3, 1))
        R_target2cam_list.append(T_target2cam[:3, :3])
        t_target2cam_list.append(T_target2cam[:3,  3].reshape(3, 1))
        captured_poses.append(current_idx + 1)

        d_cm = np.linalg.norm(T_target2cam[:3, 3])
        print(f"  ✓ 采集位置 {current_idx + 1:2d}  [{joints_src}]  "
              f"关节角=[{', '.join(f'{j:.2f}' for j in joints)}]  "
              f"标记距离={d_cm:.1f}cm")

        current_idx += 1

        # 闪烁提示
        flash = display.copy()
        flash = put_chinese_text(flash, "已保存!",
                                 (w // 2 - 70, h // 2 - 36),
                                 font_size=72, color=(0, 255, 120))
        cv2.imshow("手眼标定数据采集 - step2", flash)
        cv2.waitKey(400)

        # 驱动机械臂移到下一个位置
        current_joints = None   # 清空，等待下一次回传
        if current_idx < N_POSES:
            serial_send("1")
            arm_moving = True
            if serial_connected:
                print(f"  → 发送 1，机械臂移动到位置 {current_idx + 1}...")
        else:
            # 请求下一位（固件会回 DONE）
            serial_send("1")
            arm_moving = True

cap.release()
cv2.destroyAllWindows()
if ser is not None:
    ser.close()

# ── 保存 ─────────────────────────────────────────────────────────
n = len(R_end2base_list)
print(f"\n共采集 {n} 组数据，位置编号：{captured_poses}")

if n < 5:
    print("[错误] 数据太少（至少需要 5 组），请重新采集")
    exit(1)
if n < 10:
    print("[警告] 数据偏少，建议采集 10 组以上以确保精度")

out_path = os.path.join(OUTPUT_DIR, "handeye_data.npz")
np.savez(out_path,
         R_end2base    = np.array(R_end2base_list),
         t_end2base    = np.array(t_end2base_list),
         R_target2cam  = np.array(R_target2cam_list),
         t_target2cam  = np.array(t_target2cam_list),
         captured_poses = np.array(captured_poses))
print(f"[OK] 数据已保存到：{out_path}")
print("     下一步：运行 step3_solve_handeye.py")
