#!/usr/bin/env python3
"""
collect_teach.py  ——  示教法采集像素→机械臂坐标映射
依赖：pip install opencv-python numpy pyserial

流程（每组数据）：
  1. 机械臂在固定观测位，摄像头俯瞰桌面
  2. 把红色物块放到目标位置
  3. 按 [空格] 锁定物块像素坐标
  4. 按 [U] 让舵机卸力
  5. 用手拖动机械臂末端到可以夹住物块的位置（爪子对准、高度合适）
  6. 按 [P] 读取机械臂实际坐标，保存这组数据
  7. 按 [R] 机械臂复位回观测位，继续下一组

采集完 9 组后自动拟合仿射映射，保存到 model/calibration/teach_map.npz
"""

import cv2
import numpy as np
import serial
import time
import os
import sys

# ================================================================
#  配置
# ================================================================
SERIAL_PORT  = "COM5"
CAMERA_INDEX = 1
CAMERA_ROT   = True          # 摄像头倒装旋转 180°

_HERE    = os.path.dirname(os.path.abspath(__file__))
MAP_FILE = os.path.normpath(os.path.join(_HERE, "../../../model/calibration/teach_map.npz"))

# ── 观测位姿（采集和夹取必须相同） ──────────────────────────────
OBS_X, OBS_Y, OBS_Z, OBS_PITCH = 16.0, 0.0, -3.2, -76.1

N_SAMPLES = 9   # 采集组数

# ================================================================
#  串口控制
# ================================================================
class Arm:
    def __init__(self, port):
        self._s = serial.Serial(port, 115200, timeout=0)
        time.sleep(0.3)
        self._s.reset_input_buffer()
        self._buf = b""

    def _readline(self, timeout=8.0):
        t0 = time.time()
        while time.time() - t0 < timeout:
            if self._s.in_waiting:
                self._buf += self._s.read(self._s.in_waiting)
            if b"\n" in self._buf:
                line, self._buf = self._buf.split(b"\n", 1)
                return line.decode(errors="ignore").strip()
            time.sleep(0.01)
        return ""

    def cmd(self, c, timeout=8.0):
        self._s.write((c + "\n").encode())
        return self._readline(timeout)

    def move(self, x, y, z, pitch, dur=2000):
        r = self.cmd(f"MOVE {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} -90 90 {dur}",
                     timeout=dur / 1000 + 4)
        return r == "OK"

    def read_pos(self):
        r = self.cmd("READ_POS", timeout=3)
        try:
            v = [float(x) for x in r.split(",")]
            return tuple(v) if len(v) == 4 else None
        except Exception:
            return None

    def unload(self):
        return self.cmd("UNLOAD", timeout=3) == "OK"

    def open(self, dur=600):
        return self.cmd(f"GRIPPER_CLOSE {dur}", timeout=dur/1000+3) == "OK"

    def shutdown(self):
        self._s.close()


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
#  仿射映射拟合
# ================================================================
def fit_affine(pixels, targets):
    """
    pixels : (N,2) 像素坐标 (u,v)
    targets: (N,M) 目标值，M 列（x,y 或 x,y,z,pitch 等）
    返回 A (M,3)，使得 targets^T ≈ A @ [u,v,1]^T
    """
    N = len(pixels)
    U = np.c_[np.array(pixels, dtype=float), np.ones(N)]  # (N,3)
    R = np.array(targets, dtype=float)                     # (N,M)
    A, _, _, _ = np.linalg.lstsq(U, R, rcond=None)
    return A.T   # (M,3)


# ================================================================
#  主程序
# ================================================================
def main():
    print("=" * 58)
    print("  示教采集  ——  共需采集", N_SAMPLES, "组数据")
    print("=" * 58)
    print(f"  观测位姿: x={OBS_X}  y={OBS_Y}  z={OBS_Z}  pitch={OBS_PITCH}°")
    print()
    print("  操作说明：")
    print("    空格  ── 锁定当前检测到的红色物块像素位置")
    print("    U     ── 舵机卸力（然后用手拖动机械臂到夹取位置）")
    print("    P     ── 读取机械臂当前坐标并保存本组数据")
    print("    R     ── 机械臂复位回观测位，准备下一组")
    print("    Q     ── 提前退出")
    print("=" * 58)

    try:
        arm = Arm(SERIAL_PORT)
    except Exception as e:
        sys.exit(f"[错误] 串口连接失败: {e}")

    if arm.cmd("PING", 2) != "PONG":
        print("[警告] 未收到 PONG，请确认固件处于 PC_CONTROL 模式")

    cap = cv2.VideoCapture(CAMERA_INDEX)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

    pixels = []   # [(u, v), ...]
    robots = []   # [(x, y, z, pitch), ...]

    try:
        # 移到观测位，张开夹爪
        print(f"\n[初始化] 张开夹爪...")
        arm.open()
        print(f"[初始化] 移到观测位姿...")
        ok = arm.move(OBS_X, OBS_Y, OBS_Z, OBS_PITCH, dur=2500)
        print(f"  {'[OK]' if ok else '[警告] MOVE 失败，继续'}")
        time.sleep(0.5)

        # STATE: 'obs' | 'locked' | 'teaching' | 'done_one'
        state      = 'obs'
        locked_px  = None
        cur_cx     = None
        cur_cy     = None

        while len(pixels) < N_SAMPLES:
            n = len(pixels) + 1

            ret, frame = cap.read()
            if not ret:
                continue
            if CAMERA_ROT:
                frame = cv2.rotate(frame, cv2.ROTATE_180)

            cur_cx, cur_cy, disp = detect_red(frame)

            # ── 界面提示 ───────────────────────────────────────────
            h, w = disp.shape[:2]
            if state == 'obs':
                hint = f"[{n}/{N_SAMPLES}] Place block, press SPACE to lock"
                color = (0, 200, 255)
                if cur_cx is not None:
                    cv2.putText(disp, f"({cur_cx},{cur_cy})",
                                (cur_cx + 12, cur_cy - 12),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 255, 0), 2)
            elif state == 'locked':
                hint = f"[{n}/{N_SAMPLES}] Locked({locked_px[0]},{locked_px[1]})  U=unload  R=retry"
                color = (0, 255, 255)
                cv2.circle(disp, locked_px, 14, (0, 255, 255), 3)
            elif state == 'teaching':
                hint = f"[{n}/{N_SAMPLES}] Drag to grasp pos  P=record  R=retry"
                color = (255, 180, 0)
                cv2.circle(disp, locked_px, 14, (0, 255, 255), 3)
            else:  # done_one
                hint = f"[{n-1}/{N_SAMPLES}] Saved!  R=return to obs"
                color = (0, 255, 100)

            cv2.putText(disp, hint, (10, 36),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.78, color, 2)
            cv2.putText(disp, f"Done: {len(pixels)}/{N_SAMPLES}",
                        (10, h - 14),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.65, (200, 200, 200), 2)

            cv2.imshow("collect_teach", disp)
            key = cv2.waitKey(30) & 0xFF

            if key == ord('q'):
                print("\n提前退出")
                break

            # ── 状态转移 ────────────────────────────────────────────
            if state == 'obs':
                if key == ord(' ') and cur_cx is not None:
                    locked_px = (cur_cx, cur_cy)
                    state = 'locked'
                    print(f"\n  [{n}/{N_SAMPLES}] 锁定像素: ({cur_cx}, {cur_cy})")
                    print(f"  按 [U] 卸力，拖动到夹取位置后按 [P] 记录")

            elif state == 'locked':
                if key == ord('u'):
                    arm.unload()
                    state = 'teaching'
                    print(f"  [卸力] 舵机已松开，请拖动机械臂到夹取位置后按 [P]")
                elif key == ord('r'):
                    locked_px = None
                    state = 'obs'
                    print(f"  [重试] 重新锁定")

            elif state == 'teaching':
                if key == ord('p'):
                    pos = arm.read_pos()
                    if pos:
                        px, py, pz, pp = pos
                        pixels.append(locked_px)
                        robots.append((px, py, pz, pp))
                        print(f"  [记录 {len(pixels)}/{N_SAMPLES}]")
                        print(f"    像素  : ({locked_px[0]}, {locked_px[1]})")
                        print(f"    机械臂: x={px:.2f}  y={py:.2f}  z={pz:.2f}  pitch={pp:.1f}°")
                        state = 'done_one'
                        if len(pixels) < N_SAMPLES:
                            print(f"  按 [R] 回到观测位，继续第 {len(pixels)+1} 组")
                    else:
                        print(f"  [错误] 读取坐标失败，请再试一次（舵机可能未稳定）")
                elif key == ord('r'):
                    print(f"  [复位] 张开夹爪，回到观测位...")
                    arm.open()
                    arm.move(OBS_X, OBS_Y, OBS_Z, OBS_PITCH)
                    locked_px = None
                    state = 'obs'
                    print(f"  [OK] 继续第 {n} 组")

            elif state == 'done_one':
                if key == ord('r'):
                    print(f"  [复位] 张开夹爪，回到观测位...")
                    arm.open()
                    arm.move(OBS_X, OBS_Y, OBS_Z, OBS_PITCH)
                    time.sleep(0.3)
                    locked_px = None
                    state = 'obs'
                    print(f"  [OK] 继续第 {len(pixels)+1} 组")

        cv2.destroyAllWindows()

    finally:
        cap.release()
        cv2.destroyAllWindows()
        arm.shutdown()

    # ================================================================
    #  拟合 & 保存
    # ================================================================
    n = len(pixels)
    if n < 4:
        print(f"\n[失败] 只采集了 {n} 组，至少需要 4 组才能拟合，退出。")
        return

    print(f"\n{'='*58}")
    print(f"  拟合仿射映射（共 {n} 组数据）")
    print(f"{'='*58}")

    px_arr  = np.array(pixels, dtype=float)   # (N,2)
    rob_arr = np.array(robots, dtype=float)   # (N,4): x,y,z,pitch

    # 对 x,y,z,pitch 全部做仿射拟合
    A = fit_affine(px_arr, rob_arr)   # (4,3)

    # 验证残差
    U       = np.c_[px_arr, np.ones(n)]    # (N,3)
    pred    = (A @ U.T).T                  # (N,4)
    errs_xy = np.linalg.norm(pred[:, :2] - rob_arr[:, :2], axis=1)
    errs_z  = np.abs(pred[:, 2] - rob_arr[:, 2])
    errs_p  = np.abs(pred[:, 3] - rob_arr[:, 3])

    print(f"  仿射矩阵 A (4×3)  [x, y, z, pitch]:")
    labels = ["x    ", "y    ", "z    ", "pitch"]
    for label, row in zip(labels, A):
        print(f"    {label}: [{row[0]:+.6f}  {row[1]:+.6f}  {row[2]:+.6f}]")
    print(f"  xy残差  最大={errs_xy.max():.2f} cm  均值={errs_xy.mean():.2f} cm")
    print(f"  z残差   最大={errs_z.max():.2f} cm  均值={errs_z.mean():.2f} cm")
    print(f"  pitch残差 最大={errs_p.max():.1f}°  均值={errs_p.mean():.1f}°")

    print(f"\n  各组数据（预测 vs 实测）：")
    for i, (pix, rob, prd, exy, ez, ep) in enumerate(
            zip(pixels, rob_arr, pred, errs_xy, errs_z, errs_p)):
        print(f"    [{i+1:02d}] ({pix[0]:4d},{pix[1]:4d})"
              f"  实: x={rob[0]:6.2f} y={rob[1]:5.2f} z={rob[2]:6.2f} p={rob[3]:5.1f}°"
              f"  预: x={prd[0]:6.2f} y={prd[1]:5.2f} z={prd[2]:6.2f} p={prd[3]:5.1f}°"
              f"  |xy|={exy:.2f}cm z={ez:.2f}cm p={ep:.1f}°")

    os.makedirs(os.path.dirname(MAP_FILE), exist_ok=True)
    np.savez(MAP_FILE,
             A=A,
             obs_pose=np.array([OBS_X, OBS_Y, OBS_Z, OBS_PITCH]),
             pixels=px_arr,
             robots=rob_arr)

    print(f"\n[保存] → {MAP_FILE}")
    print(f"{'='*58}")


if __name__ == "__main__":
    main()
