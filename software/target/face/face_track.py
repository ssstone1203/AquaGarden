#!/usr/bin/env python3
"""
face_track.py  ——  人脸追踪：机械臂跟随人脸保持居中
依赖：pip install opencv-python pyserial

逻辑：
  TRACKING  : 检测到人脸 → 计算人脸偏离画面中心的像素差 → 调整底座旋转角使脸居中
  SCANNING  : 未检测到人脸 → 左右扫描两圈寻找人脸
  NO_FACE   : 扫完两圈仍无人脸 → 画面左上角显示 "No Face"，继续低频扫描

控制方式：仅旋转底座（改变底座角），臂长 TRACK_R 保持不变，
          x = TRACK_R * cos(angle)，y = TRACK_R * sin(angle)
"""

import cv2
import math
import serial
import time
import sys

# ================================================================
#  硬件配置
# ================================================================
SERIAL_PORT  = "COM14"
CAMERA_INDEX = 1
CAMERA_ROT   = True   # 摄像头倒装旋转 180°

# ================================================================
#  机械臂固定参数（来自示教，z / pitch / 臂长 保持不变）
# ================================================================
TRACK_Z     = 21.08    # cm
TRACK_PITCH = -1.0     # °
TRACK_R     = 16.52    # cm，臂长（底座旋转半径）；= sqrt(x²+y²) at center

ANGLE_MIN    = -50.9   # 底座向右极限（°）
ANGLE_MAX    =  52.1   # 底座向左极限（°）
ANGLE_CENTER =  -1.7   # 底座中心位置（°）

# ================================================================
#  追踪参数
# ================================================================
# 人脸像素偏移 → 底座角修正量：像素偏移 * 比例 = 角度修正（°）
TRACK_SCALE     =  0.030  # °/px；正值=偏右时角度增大（向左转）
ANGLE_DIRECTION =  -1      # 1 正向，-1 反向（若追踪方向反了改这里）
TRACK_DEADZONE  =  50     # px，小于此偏移不动（适当加大避免抖动）
TRACK_DUR_MS    =  400    # ms，追踪时每次调整移动时长
TRACK_COOLDOWN  =  0.6    # s，两次移动之间的最短间隔（防止帧缓冲导致连续触发）

# ================================================================
#  扫描参数
# ================================================================
SCAN_STEP     = 4.0   # °，每步旋转量（越小越细腻，越慢）
SCAN_DUR_MS   = 150   # ms，每步移动时长
SCAN_CYCLES   = 2     # 未检测到人脸时扫描的完整圈数（左→右为一圈）

# ================================================================
#  人脸检测器
# ================================================================
_CASCADE_PATH = cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
cascade = cv2.CascadeClassifier(_CASCADE_PATH)
if cascade.empty():
    sys.exit("[错误] haarcascade 文件加载失败，请检查 OpenCV 安装")


# ================================================================
#  角度 ↔ 坐标 转换
# ================================================================
def angle_to_xy(angle_deg):
    """底座角度（°）→ (x, y)，臂长固定为 TRACK_R"""
    rad = math.radians(angle_deg)
    return TRACK_R * math.cos(rad), TRACK_R * math.sin(rad)


# ================================================================
#  串口控制
# ================================================================
class Arm:
    def __init__(self, port):
        self._s = serial.Serial(port, 115200, timeout=0)
        time.sleep(0.3)
        self._s.reset_input_buffer()
        self._buf = b""

    def _readline(self, timeout=6.0):
        t0 = time.time()
        while time.time() - t0 < timeout:
            if self._s.in_waiting:
                self._buf += self._s.read(self._s.in_waiting)
            if b"\n" in self._buf:
                line, self._buf = self._buf.split(b"\n", 1)
                return line.decode(errors="ignore").strip()
            time.sleep(0.01)
        return ""

    def cmd(self, c, timeout=6.0):
        self._s.write((c + "\n").encode())
        return self._readline(timeout)

    def move_angle(self, angle_deg, z, pitch, dur=500):
        """仅旋转底座：由角度算出 x/y 后发送 MOVE 指令"""
        angle_deg = max(ANGLE_MIN, min(ANGLE_MAX, angle_deg))
        x, y = angle_to_xy(angle_deg)
        r = self.cmd(
            f"MOVE {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} -90 90 {dur}",
            timeout=dur / 1000 + 3,
        )
        return r == "OK"

    def shutdown(self):
        self._s.close()


# ================================================================
#  人脸检测
# ================================================================
def detect_face(frame):
    """
    返回最大人脸的 (cx, cy, w, h)，无人脸时返回 None。
    cx/cy 为人脸矩形中心像素坐标。
    """
    gray  = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = cascade.detectMultiScale(
        gray, scaleFactor=1.1, minNeighbors=5, minSize=(60, 60)
    )
    if len(faces) == 0:
        return None
    # 取面积最大的一张脸
    x, y, w, h = max(faces, key=lambda f: f[2] * f[3])
    return int(x + w / 2), int(y + h / 2), w, h


# ================================================================
#  生成扫描路径（中间 → 右端 → 左端 × SCAN_CYCLES，角度单位°）
# ================================================================
def make_scan_waypoints():
    points = []
    angle = ANGLE_CENTER
    for _ in range(SCAN_CYCLES):
        # 向右扫（角度减小）
        while angle > ANGLE_MIN:
            angle -= SCAN_STEP
            points.append(max(ANGLE_MIN, angle))
        # 向左扫（角度增大）
        while angle < ANGLE_MAX:
            angle += SCAN_STEP
            points.append(min(ANGLE_MAX, angle))
    points.append(ANGLE_CENTER)   # 结束后回中
    return points


# ================================================================
#  主程序
# ================================================================
def main():
    print("=" * 52)
    print("  人脸追踪  face_track.py  （底座旋转模式）")
    print("=" * 52)
    print(f"  z={TRACK_Z}  pitch={TRACK_PITCH}°  臂长={TRACK_R}cm  固定不变")
    print(f"  底座角范围: [{ANGLE_MIN}°, {ANGLE_MAX}°]  中心={ANGLE_CENTER}°")
    print(f"  按 Q 退出")
    print("=" * 52)

    arm = Arm(SERIAL_PORT)
    arm.cmd("PING", 2)

    cap = cv2.VideoCapture(CAMERA_INDEX)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)   # 最小化驱动层帧缓冲

    # 先移到中心位置
    print("\n[初始化] 移到追踪起始位置...")
    arm.move_angle(ANGLE_CENTER, TRACK_Z, TRACK_PITCH, dur=2000)
    time.sleep(2.2)

    current_angle  = ANGLE_CENTER
    last_move_time = 0.0          # 上次发送移动指令的时间戳
    STATE = "TRACKING"            # TRACKING / SCANNING / NO_FACE

    scan_points  = []
    scan_idx     = 0
    no_face_flag = False

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                continue
            if CAMERA_ROT:
                frame = cv2.rotate(frame, cv2.ROTATE_180)

            h_img, w_img = frame.shape[:2]
            cx_img = w_img // 2   # 画面水平中心

            face = detect_face(frame)

            # ── 绘制画面中心线 ──────────────────────────────────
            cv2.line(frame, (cx_img, 0), (cx_img, h_img), (100, 100, 100), 1)

            if face is not None:
                fx, fy, fw, fh = face
                # 画框
                cv2.rectangle(frame,
                               (fx - fw//2, fy - fh//2),
                               (fx + fw//2, fy + fh//2),
                               (0, 220, 0), 2)
                cv2.circle(frame, (fx, fy), 6, (0, 255, 0), -1)

                offset_px = fx - cx_img    # 正=偏右，负=偏左
                cv2.putText(frame, f"offset={offset_px:+d}px",
                            (10, 80), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 220, 0), 2)

                # 切换到追踪模式，重置扫描状态
                STATE        = "TRACKING"
                scan_points  = []
                scan_idx     = 0
                no_face_flag = False

                # 只有偏移超过死区，且冷却时间已过，才调整
                now = time.time()
                if abs(offset_px) > TRACK_DEADZONE and (now - last_move_time) >= TRACK_COOLDOWN:
                    d_angle = offset_px * TRACK_SCALE * ANGLE_DIRECTION
                    new_angle = max(ANGLE_MIN, min(ANGLE_MAX, current_angle + d_angle))
                    if abs(new_angle - current_angle) > 0.1:
                        arm.move_angle(new_angle, TRACK_Z, TRACK_PITCH,
                                       dur=TRACK_DUR_MS)
                        current_angle  = new_angle
                        last_move_time = time.time()
                        # 清空摄像头帧缓冲，下一帧取最新画面
                        for _ in range(4):
                            cap.grab()

                cv2.putText(frame, f"TRACKING  angle={current_angle:.1f}deg",
                            (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.85, (0, 220, 0), 2)

            else:
                # ── 无人脸 ──────────────────────────────────────
                if STATE == "TRACKING":
                    # 刚丢失人脸，进入扫描
                    STATE       = "SCANNING"
                    scan_points = make_scan_waypoints()
                    scan_idx    = 0
                    print("\n[扫描] 未检测到人脸，开始扫描...")

                if STATE == "SCANNING":
                    if scan_idx < len(scan_points):
                        target_angle  = scan_points[scan_idx]
                        scan_idx     += 1
                        arm.move_angle(target_angle, TRACK_Z, TRACK_PITCH,
                                       dur=SCAN_DUR_MS)
                        current_angle = target_angle

                        cv2.putText(frame,
                                    f"SCANNING  {scan_idx}/{len(scan_points)}  angle={current_angle:.1f}deg",
                                    (10, 40), cv2.FONT_HERSHEY_SIMPLEX,
                                    0.8, (0, 180, 255), 2)
                    else:
                        # 扫描结束，依然无脸
                        STATE        = "NO_FACE"
                        no_face_flag = True
                        print("[无脸] 扫描完两圈未检测到人脸")

                if STATE == "NO_FACE":
                    cv2.putText(frame, "No Face",
                                (10, 40), cv2.FONT_HERSHEY_SIMPLEX,
                                1.2, (0, 0, 255), 3)
                    # 每隔一段时间重新扫一遍
                    if scan_idx == 0:
                        scan_points = make_scan_waypoints()
                    # 继续低频扫描（每帧推进一步，但 SCAN_DUR_MS 更长）
                    if scan_idx < len(scan_points):
                        target_angle  = scan_points[scan_idx]
                        scan_idx     += 1
                        arm.move_angle(target_angle, TRACK_Z, TRACK_PITCH,
                                       dur=SCAN_DUR_MS * 2)
                        current_angle = target_angle
                    else:
                        scan_idx = 0   # 重新循环

            cv2.putText(frame, "Q=quit",
                        (10, h_img - 14),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (180, 180, 180), 1)
            cv2.imshow("face_track", frame)

            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                print("退出")
                break

    finally:
        cap.release()
        cv2.destroyAllWindows()
        arm.shutdown()


if __name__ == "__main__":
    main()
