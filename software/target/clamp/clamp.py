#!/usr/bin/env python3
"""
clamp.py  ——  红色物块夹取（示教映射版）
依赖：pip install opencv-python numpy pyserial
"""

import cv2
import numpy as np
import serial
import time
import os
import sys

# ================================================================
#  硬件配置
# ================================================================
SERIAL_PORT  = "COM5"
CAMERA_INDEX = 1
CAMERA_ROT   = True          # 摄像头倒装旋转 180°

_HERE    = os.path.dirname(os.path.abspath(__file__))
MAP_FILE = os.path.normpath(os.path.join(_HERE, "../../../model/calibration/teach_map.npz"))

# ================================================================
#  ★  观测位姿（必须与 collect_teach.py 完全一致）  ★
# ================================================================
OBS_X, OBS_Y, OBS_Z, OBS_PITCH = 16.0, 0.0, -3.2, -76.1

# ================================================================
#  夹取参数（会被 teach_map.npz 中的均值覆盖）
# ================================================================
GRASP_Z     = -9.59   # 夹爪闭合时的 z（cm）
GRASP_PITCH = -81.6   # 夹取俯仰角

ABOVE_CLEARANCE = 1.0              # 接近点：物块顶面上方（cm）
BLOCK_TOP_Z     = GRASP_Z + 1.5   # 物块顶面估算（cm），仅用于计算 ABOVE_Z
LIFT_Z          = -2.0             # 抬起后的 z（cm）

# 水平夹持位姿（夹起后姿态调整）
HORIZ_X, HORIZ_Y, HORIZ_Z, HORIZ_PITCH = 15.0, 0.0, -5.0, 0.0

# ================================================================
#  串口控制
# ================================================================
class Arm:
    def __init__(self, port):
        self._s = serial.Serial(port, 115200, timeout=0)
        time.sleep(0.3)
        self._s.reset_input_buffer()
        self._buf = b""

    def _readline(self, timeout=10.0):
        t0 = time.time()
        while time.time() - t0 < timeout:
            if self._s.in_waiting:
                self._buf += self._s.read(self._s.in_waiting)
            if b"\n" in self._buf:
                line, self._buf = self._buf.split(b"\n", 1)
                return line.decode(errors="ignore").strip()
            time.sleep(0.01)
        return ""

    def cmd(self, c, timeout=10.0):
        self._s.write((c + "\n").encode())
        r = self._readline(timeout)
        print(f"  >> {c}\n  << {r or '(超时)'}")
        return r

    def move(self, x, y, z, pitch, dur=1500, mn=-90, mx=90):
        return self.cmd(f"MOVE {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} {mn} {mx} {dur}",
                        timeout=dur / 1000 + 4) == "OK"

    def read_pos(self):
        r = self.cmd("READ_POS", timeout=3)
        try:
            v = [float(x) for x in r.split(",")]
            return tuple(v) if len(v) == 4 else None
        except Exception:
            return None

    def open(self,  dur=600): return self.cmd(f"GRIPPER_CLOSE {dur}", timeout=dur/1000+3) == "OK"
    def close(self, dur=600): return self.cmd(f"GRIPPER_OPEN  {dur}", timeout=dur/1000+3) == "OK"
    def shutdown(self): self._s.close()


# ================================================================
#  加载示教映射
# ================================================================
def load_map():
    if not os.path.exists(MAP_FILE):
        sys.exit(f"[错误] 找不到示教映射文件:\n  {MAP_FILE}\n  请先运行 collect_teach.py 采集数据。")
    data = np.load(MAP_FILE)
    A_xy       = data["A_xy"]          # (2,3) 仿射矩阵
    mean_z     = float(data["mean_z"])
    mean_pitch = float(data["mean_pitch"])
    obs_pose   = data.get("obs_pose", None)
    return A_xy, mean_z, mean_pitch, obs_pose


def pixel_to_robot(u, v, A_xy):
    """像素坐标 → 机械臂 (x, y)"""
    uv1 = np.array([u, v, 1.0])
    xy  = A_xy @ uv1
    return float(xy[0]), float(xy[1])


# ================================================================
#  红色物块检测
# ================================================================
def detect_red(frame):
    hsv  = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    m1   = cv2.inRange(hsv, np.array([0,   80, 80]), np.array([10,  255, 255]))
    m2   = cv2.inRange(hsv, np.array([160, 80, 80]), np.array([180, 255, 255]))
    mask = cv2.morphologyEx(m1 | m2, cv2.MORPH_OPEN,   np.ones((5, 5), np.uint8))
    mask = cv2.morphologyEx(mask,    cv2.MORPH_DILATE,  np.ones((3, 3), np.uint8))
    cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    disp = frame.copy()
    if cnts:
        c = max(cnts, key=cv2.contourArea)
        if cv2.contourArea(c) > 300:
            M = cv2.moments(c)
            if M["m00"] > 0:
                cx = int(M["m10"] / M["m00"])
                cy = int(M["m01"] / M["m00"])
                cv2.drawContours(disp, [c], -1, (0, 0, 255), 2)
                cv2.circle(disp, (cx, cy), 8, (0, 255, 0), -1)
                return cx, cy, disp
    return None, None, disp


# ================================================================
#  主流程
# ================================================================
def main():
    # 加载示教映射
    A_xy, gz, gp, obs_pose = load_map()

    # 允许在文件顶部覆盖 z/pitch（若用户手动修改了 GRASP_Z）
    # 否则使用 teach_map 中的均值
    grasp_z     = gz
    grasp_pitch = gp
    above_z     = grasp_z + ABOVE_CLEARANCE + 1.5   # 接近点：夹取深度上方约 3cm

    print("=" * 52)
    print("  红色物块夹取  ——  示教映射版")
    print("=" * 52)
    print(f"  夹取 z     = {grasp_z:.2f} cm")
    print(f"  夹取 pitch = {grasp_pitch:.1f}°")
    print(f"  接近高度   = {above_z:.2f} cm")
    if obs_pose is not None:
        print(f"  观测位姿   = ({obs_pose[0]},{obs_pose[1]},{obs_pose[2]},p={obs_pose[3]}°)")
    print("=" * 52)

    arm = Arm(SERIAL_PORT)
    arm.cmd("PING", timeout=2)

    cap = cv2.VideoCapture(CAMERA_INDEX)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

    try:
        # ── 步骤1：移到观测位姿 ─────────────────────────────────────
        print(f"\n[步骤1] 移到观测位姿...")
        arm.move(OBS_X, OBS_Y, OBS_Z, OBS_PITCH, dur=2500)

        # ── 步骤2：检测物块，按空格确认 ─────────────────────────────
        print(f"\n[步骤2] 检测红色物块 — 按 [空格] 确认，[Q] 退出")
        bx = by = None

        while True:
            ret, frame = cap.read()
            if not ret:
                continue
            if CAMERA_ROT:
                frame = cv2.rotate(frame, cv2.ROTATE_180)

            cx, cy, disp = detect_red(frame)

            if cx is not None:
                rx, ry = pixel_to_robot(cx, cy, A_xy)
                cv2.putText(disp, f"x={rx:.1f}  y={ry:.1f} cm",
                            (10, 80), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 220, 0), 2)
                cv2.putText(disp, f"pixel ({cx},{cy})",
                            (10, 116), cv2.FONT_HERSHEY_SIMPLEX, 0.65, (180, 180, 180), 1)

            cv2.putText(disp, "SPACE=confirm  Q=quit",
                        (10, disp.shape[0] - 12),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (180, 180, 180), 1)
            cv2.imshow("clamp", disp)
            key = cv2.waitKey(30) & 0xFF

            if key == ord('q'):
                print("退出")
                return
            if key == ord(' ') and cx is not None:
                bx, by = pixel_to_robot(cx, cy, A_xy)
                print(f"\n  像素坐标:   ({cx}, {cy})")
                print(f"  机械臂坐标: x={bx:.2f}  y={by:.2f}  z={grasp_z:.2f}")
                break

        cv2.destroyAllWindows()
        if bx is None:
            return

        # ── 步骤3：夹取 ────────────────────────────────────────────
        print(f"\n[步骤3] 夹取")

        print(f"  (a) 张开夹爪")
        arm.open()

        print(f"  (b) 移到物块上方  ({bx:.2f}, {by:.2f}, {above_z:.2f})")
        if not arm.move(bx, by, above_z, grasp_pitch, dur=1500):
            print("  [错误] 移动失败")
            return

        print(f"  (c) 下降至夹取深度  z={grasp_z:.2f}")
        if not arm.move(bx, by, grasp_z, grasp_pitch, dur=1500):
            print("  [错误] 移动失败")
            return

        print(f"  (d) 闭合夹爪")
        arm.close()
        time.sleep(0.4)

        print(f"  (e) 抬起  z={LIFT_Z:.2f}")
        if not arm.move(bx, by, LIFT_Z, grasp_pitch, dur=1200):
            print("  [错误] 移动失败")
            return

        # ── 步骤4：水平夹持 ─────────────────────────────────────────
        print(f"\n[步骤4] 调整水平夹持  ({HORIZ_X},{HORIZ_Y},{HORIZ_Z},p={HORIZ_PITCH}°)")
        arm.move(HORIZ_X, HORIZ_Y, HORIZ_Z, HORIZ_PITCH, dur=3000)

        print("\n" + "=" * 52)
        print("  完成！")
        print("=" * 52)
        input("按 Enter 退出...")

    finally:
        cap.release()
        cv2.destroyAllWindows()
        arm.shutdown()


if __name__ == "__main__":
    main()
