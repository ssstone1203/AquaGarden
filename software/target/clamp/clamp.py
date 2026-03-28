#!/usr/bin/env python3
"""
clamp.py  ——  彩色物块夹取分拣（示教映射版）
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
SERIAL_PORT  = "COM14"
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
LIFT_Z          = -2.0             # 保留备用（已不用于主流程）

# 视觉映射的 z 略偏下时，合拢易蹭桌面：夹取终点在映射 z 基础上抬高（cm），Z 越大越高
GRASP_Z_LIFT   = 0.8

# ★ 安全运输高度：所有水平移动必须在此高度以上进行 ★
# 取值依据：工作区内最高物块顶面约 z = -6 cm，
# 再留 10 cm 净空（物块 + 夹爪延伸量），取 4.0 cm
SAFE_TRANSPORT_Z = 4.0

# 水平夹持过渡位姿（夹起后先在此调整姿态，z 固定为 SAFE_TRANSPORT_Z）
HORIZ_X, HORIZ_Y, HORIZ_PITCH = 14.68, 0.25, -54.7

# ================================================================
#  放置区坐标（松开夹爪的位置）
#  按键：R = 红区  G = 绿区  B = 蓝区
# ================================================================
ZONES = {
    'r': dict(name="Red   zone", x=  8.31, y= 21.78, z= -6.54, pitch= -74.2),
    'g': dict(name="Green zone", x=  0.84, y= 22.21, z= -7.01, pitch= -78.0),
    'b': dict(name="Blue  zone", x= -6.36, y= 21.34, z= -7.24, pitch= -72.2),
}

# ================================================================
#  ★  微调偏差（映射结果的常量修正）  ★
# ================================================================
X_BIAS =  -0.2   # cm，正=靠前 负=靠后（超声波校正启用时仅影响相机估算的 fallback）
Y_BIAS =  0.3   # cm，正=偏左 负=偏右

# ================================================================
#  超声波 X 轴距离校正 
# ================================================================
# 超声波安装在基座与物块之间，沿 X 轴正方向测距。
# 几何关系：bx_real = SENSOR_X_OFFSET + 超声波读数
#
# 传感器安装位置到机械臂基座原点的 X 轴距离（实测 8.7 cm，误差 ±0.6 cm）
SENSOR_X_OFFSET = 8.7   # cm

# 超声波与相机估算的最大允许偏差（cm）；超出此范围则认为测量异常，回退到相机估算
# 设为测量误差（0.6）+ 相机映射残差裕量（1.5）= 2.1，取整为 2.5
ULTRA_SANITY_RANGE = 4.0

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

    def move(self, x, y, z, pitch, dur=1000, mn=-90, mx=90):
        return self.cmd(f"MOVE {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} {mn} {mx} {dur}",
                        timeout=dur / 1000 + 4) == "OK"

    def read_pos(self):
        r = self.cmd("READ_POS", timeout=3)
        try:
            v = [float(x) for x in r.split(",")]
            return tuple(v) if len(v) == 4 else None
        except Exception:
            return None

    def dist(self, timeout=3.0):
        """发送 DIST 指令，返回超声波测距结果 (cm)；失败返回 -1.0"""
        r = self.cmd("DIST", timeout=timeout)
        try:
            v = float(r)
            return v if v > 0 else -1.0
        except Exception:
            return -1.0

    def open(self,  dur=600): return self.cmd(f"GRIPPER_CLOSE {dur}", timeout=dur/1000+3) == "OK"
    def close(self, dur=600): return self.cmd(f"GRIPPER_OPEN  {dur}", timeout=dur/1000+3) == "OK"
    def shutdown(self): self._s.close()


# ================================================================
#  加载示教映射
# ================================================================
def load_map():
    if not os.path.exists(MAP_FILE):
        sys.exit(f"[错误] 找不到示教映射文件:\n  {MAP_FILE}\n  请先运行 collect_teach.py 采集数据。")
    data     = np.load(MAP_FILE)
    A        = data["A"]               # (4,3): x,y,z,pitch
    obs_pose = data.get("obs_pose", None)
    return A, obs_pose


def pixel_to_robot(u, v, A):
    """像素坐标 → 机械臂 (x, y, z, pitch)，含偏差补偿"""
    uv1  = np.array([u, v, 1.0])
    xyzp = A @ uv1
    return float(xyzp[0]) + X_BIAS, float(xyzp[1]) + Y_BIAS, float(xyzp[2]), float(xyzp[3])


# ================================================================
#  彩色物块检测（自动识别红/绿/蓝）
# ================================================================
_COLOR_RANGES = {
    'r': [(np.array([0,   80, 80]), np.array([10,  255, 255])),
          (np.array([160, 80, 80]), np.array([180, 255, 255]))],
    'g': [(np.array([40,  60, 60]), np.array([85,  255, 255]))],
    'b': [(np.array([95, 50, 40]), np.array([135, 255, 255]))],
}
_COLOR_BGR = {'r': (0, 0, 255), 'g': (0, 200, 0), 'b': (255, 80, 0)}
_COLOR_NAME = {'r': 'Red', 'g': 'Green', 'b': 'Blue'}

def detect_block(frame):
    """
    检测画面中最大的彩色物块。
    返回 (cx, cy, color_key, disp)，未检测到时 cx=cy=color_key=None。
    """
    hsv  = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    kern = np.ones((5, 5), np.uint8)
    best_area = 300
    best = (None, None, None)

    for key, ranges in _COLOR_RANGES.items():
        mask = np.zeros(hsv.shape[:2], dtype=np.uint8)
        for lo, hi in ranges:
            mask |= cv2.inRange(hsv, lo, hi)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN,   kern)
        mask = cv2.morphologyEx(mask, cv2.MORPH_DILATE, kern)
        cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not cnts:
            continue
        c = max(cnts, key=cv2.contourArea)
        area = cv2.contourArea(c)
        if area > best_area:
            M = cv2.moments(c)
            if M["m00"] > 0:
                best_area = area
                cx = int(M["m10"] / M["m00"])
                cy = int(M["m01"] / M["m00"])
                best = (cx, cy, key, c)

    disp = frame.copy()
    if best[0] is not None:
        cx, cy, key, contour = best
        bgr = _COLOR_BGR[key]
        cv2.drawContours(disp, [contour], -1, bgr, 2)
        cv2.circle(disp, (cx, cy), 8, (0, 255, 0), -1)
        cv2.putText(disp, _COLOR_NAME[key], (cx + 12, cy - 12),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, bgr, 2)
        return cx, cy, key, disp
    return None, None, None, disp


# ================================================================
#  主流程
# ================================================================
def main():
    # 加载示教映射
    A, obs_pose = load_map()

    print("=" * 52)
    print("  彩色物块夹取分拣  ——  示教映射版")
    print("=" * 52)
    if obs_pose is not None:
        print(f"  观测位姿 = ({obs_pose[0]},{obs_pose[1]},{obs_pose[2]},p={obs_pose[3]}°)")
    print("  自动识别红/绿/蓝，放入对应区域")
    print("=" * 52)

    arm = Arm(SERIAL_PORT)
    arm.cmd("PING", timeout=2)

    cap = cv2.VideoCapture(CAMERA_INDEX)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

    try:
        # ── 步骤1：移到观测位姿 ─────────────────────────────────────
        print(f"\n[步骤1] 移到观测位姿...")
        arm.move(OBS_X, OBS_Y, OBS_Z, OBS_PITCH, dur=1500)

        # ── 步骤2：检测物块，按空格确认 ─────────────────────────────
        print(f"\n[步骤2] 检测彩色物块 — 按 [空格] 确认，[Q] 退出")
        bx = by = bz = bp = None
        block_color = None

        while True:
            ret, frame = cap.read()
            if not ret:
                continue
            if CAMERA_ROT:
                frame = cv2.rotate(frame, cv2.ROTATE_180)

            cx, cy, color_key, disp = detect_block(frame)

            if cx is not None:
                rx, ry, rz, rp = pixel_to_robot(cx, cy, A)
                cv2.putText(disp, f"x={rx:.1f} y={ry:.1f} z={rz:.1f} p={rp:.1f}",
                            (10, 80), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 220, 0), 2)
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
                bx, by, bz, bp = pixel_to_robot(cx, cy, A)
                block_color = color_key

                # ── 超声波 X 轴校正 ──────────────────────────────────
                # 此时机械臂静止在观测位姿，传感器正对物块，读数最可靠
                d = arm.dist()
                bx_camera = bx
                if d > 0:
                    bx_ultra = SENSOR_X_OFFSET + d
                    if abs(bx_ultra - bx_camera) <= ULTRA_SANITY_RANGE:
                        bx = bx_ultra
                        print(f"\n  超声波校正: {d:.2f} cm  "
                              f"(相机估算 {bx_camera:.2f} → 校正后 {bx:.2f})")
                    else:
                        print(f"\n  超声波读数 {bx_ultra:.2f} cm 偏差过大 "
                              f"（相机估算 {bx_camera:.2f}），使用相机估算")
                else:
                    print(f"\n  超声波测距失败，使用相机估算 x={bx_camera:.2f}")
                # ─────────────────────────────────────────────────────

                above_z = bz + ABOVE_CLEARANCE + 1.5
                print(f"  检测颜色:   {_COLOR_NAME.get(color_key, '未知')}")
                print(f"  像素坐标:   ({cx}, {cy})")
                print(f"  最终夹取:   x={bx:.2f}  y={by:.2f}  z={bz:.2f}  pitch={bp:.1f}°")
                print(f"  接近高度:   z={above_z:.2f} cm")
                break

        cv2.destroyAllWindows()
        if bx is None:
            return

        # ── 步骤3：夹取 ────────────────────────────────────────────
        print(f"\n[步骤3] 夹取")

        grasp_z = bz + GRASP_Z_LIFT

        print(f"  (a) 张开夹爪")
        arm.open()

        above_z = bz + ABOVE_CLEARANCE + 1.5
        print(f"  (b) 移到物块上方  ({bx:.2f}, {by:.2f}, {above_z:.2f})")
        if not arm.move(bx, by, above_z, bp, dur=1000):
            print("  [错误] 移动失败")
            return

        print(f"  (c) 下降至夹取深度  z={grasp_z:.2f}  (映射 z={bz:.2f} + 抬高 {GRASP_Z_LIFT})  pitch={bp:.1f}°")
        if not arm.move(bx, by, grasp_z, bp, dur=600):
            print("  [错误] 移动失败")
            return

        print(f"  (d) 闭合夹爪")
        arm.close()
        time.sleep(0.4)

        print(f"  (e) 垂直抬起至安全运输高度  z={SAFE_TRANSPORT_Z:.2f}")
        if not arm.move(bx, by, SAFE_TRANSPORT_Z, bp, dur=800):
            print("  [错误] 抬起失败")
            return

        # ── 步骤4：在安全高度调整夹持姿态 ─────────────────────────
        # 保持 z = SAFE_TRANSPORT_Z，只改变 x/y 和 pitch，不做任何下降
        print(f"\n[步骤4] 安全高度内调整夹持姿态  z={SAFE_TRANSPORT_Z:.2f}")
        arm.move(HORIZ_X, HORIZ_Y, SAFE_TRANSPORT_Z, HORIZ_PITCH, dur=1000)

        # ── 步骤5：自动选区（按物块颜色） ───────────────────────────
        zone = ZONES.get(block_color)
        if zone is None:
            print(f"\n[步骤5] 未识别颜色（{block_color}），跳过放置")
        else:
            print(f"\n[步骤5] 自动选区: {zone['name']}  "
                  f"({zone['x']},{zone['y']},{zone['z']},p={zone['pitch']}°)")

        # ── 步骤6：平移到放置区正上方（安全高度）→ 下降放置 ─────────
        # 规则：全程 z ≥ SAFE_TRANSPORT_Z，只有到达目标 x/y 正上方才下降
        if zone:
            # 放置区上方的安全停靠高度（取安全运输高度与放置点+余量的较大值）
            PLACE_CLEARANCE = 3.0
            above_zone_z = max(SAFE_TRANSPORT_Z, zone['z'] + PLACE_CLEARANCE)

            print(f"\n[步骤6] 水平移至放置区正上方  "
                  f"({zone['x']:.2f},{zone['y']:.2f},z={above_zone_z:.2f})")
            if not arm.move(zone['x'], zone['y'], above_zone_z,
                            zone['pitch'], dur=1400):
                print("  [错误] 移至放置区上方失败，物块未放置")
            else:
                print(f"  垂直下降到放置高度  z={zone['z']:.2f}  pitch={zone['pitch']:.1f}°")
                if arm.move(zone['x'], zone['y'], zone['z'],
                            zone['pitch'], dur=800):
                    time.sleep(0.3)
                    print(f"  松开夹爪")
                    arm.open()
                    time.sleep(0.4)
                    print(f"  垂直抬起离开  z={above_zone_z:.2f}")
                    arm.move(zone['x'], zone['y'], above_zone_z,
                             zone['pitch'], dur=700)
                else:
                    print("  [错误] 下降失败，物块未放置")

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
