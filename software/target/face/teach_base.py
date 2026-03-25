#!/usr/bin/env python3
"""
teach_base.py  ——  底座旋转自由度示教工具
用于确定 face_track.py 所需的 Y_MIN / Y_MAX / Y_CENTER 等参数

操作流程：
  1. 运行脚本，机械臂自动移到当前 z / pitch 高度
  2. 按 [U] 卸力，用手旋转底座到左/右极限
  3. 按 [L] 记录左极限，按 [R] 记录右极限，按 [C] 记录中心
  4. 按 [P] 随时查看当前坐标和底座角度
  5. 按 [Q] 退出，脚本打印可直接复制到 face_track.py 的参数
"""

import serial
import time
import math
import sys

SERIAL_PORT = "COM5"
BAUDRATE    = 115200

# 示教时机械臂保持的 z / pitch（抬高到合适位置）
TEACH_X     = 16.65
TEACH_Z     = 21.14
TEACH_PITCH = -1.0


# ================================================================
#  串口工具
# ================================================================
class Arm:
    def __init__(self, port):
        self._s = serial.Serial(port, BAUDRATE, timeout=0)
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

    def move(self, x, y, z, pitch, dur=2000):
        return self.cmd(
            f"MOVE {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} -90 90 {dur}",
            timeout=dur / 1000 + 4,
        ) == "OK"

    def read_pos(self):
        resp = self.cmd("READ_POS", timeout=3.0)
        try:
            v = [float(s) for s in resp.split(",")]
            return tuple(v) if len(v) == 4 else None
        except Exception:
            return None

    def unload(self):
        return self.cmd("UNLOAD", timeout=3) == "OK"

    def reset(self):
        return self.cmd("RESET", timeout=4)

    def shutdown(self):
        self._s.close()


# ================================================================
#  辅助：计算底座旋转角（°）
# ================================================================
def base_angle(x, y):
    """底座旋转角 = atan2(y, x)，单位度"""
    return math.degrees(math.atan2(y, x))


# ================================================================
#  主程序
# ================================================================
def main():
    print("=" * 58)
    print("  底座旋转示教  ——  face_track.py 参数获取工具")
    print("=" * 58)
    print()
    print("  [u]  卸力  —— 所有舵机松开，可用手旋转底座")
    print("  [r]  复位  —— 机械臂回到初始示教高度并上力")
    print("  [p]  读坐标 —— 显示当前 x/y/z/pitch 和底座角度")
    print("  [l]  记录左极限（y 最小值）")
    print("  [c]  记录中心位置")
    print("  [ri] 记录右极限（y 最大值）  输入 ri 回车")
    print("  [q]  退出并打印 face_track.py 参数")
    print()
    print("-" * 58)

    try:
        arm = Arm(SERIAL_PORT)
    except serial.SerialException as e:
        sys.exit(f"[错误] 串口连接失败: {e}")

    if arm.cmd("PING", 2) != "PONG":
        print("[警告] 未收到 PONG，请确认固件处于 PC_CONTROL 模式")

    # 移到示教高度
    print(f"\n[初始化] 移到示教位置 x={TEACH_X} z={TEACH_Z} pitch={TEACH_PITCH}°...")
    ok = arm.move(TEACH_X, 0.0, TEACH_Z, TEACH_PITCH, dur=2500)
    print(f"  {'[OK]' if ok else '[警告] MOVE 失败，继续'}")

    y_left   = None
    y_right  = None
    y_center = None

    def show_pos():
        pos = arm.read_pos()
        if pos:
            x, y, z, pitch = pos
            angle = base_angle(x, y)
            print(f"  x={x:.2f}  y={y:.2f}  z={z:.2f}  pitch={pitch:.1f}°  "
                  f"底座角={angle:.1f}°")
            return pos
        else:
            print("  [错误] 读取失败（舵机卸力时可能无法响应，轻移后重试）")
            return None

    try:
        while True:
            key = input("\n按键 > ").strip().lower()

            if key == "q":
                break

            elif key == "u":
                if arm.unload() :
                    print("  [OK] 舵机已卸力，可用手旋转底座")
                else:
                    print("  [错误] 卸力失败")

            elif key == "r":
                print("  [复位] 移回示教位置...")
                arm.move(TEACH_X, y_center if y_center else 0.0,
                         TEACH_Z, TEACH_PITCH, dur=2000)
                print("  [OK] 已上力并复位")

            elif key == "p":
                show_pos()

            elif key == "l":
                pos = show_pos()
                if pos:
                    y_left = pos[1]
                    print(f"  [记录] 左极限 y = {y_left:.2f} cm  "
                          f"（底座角={base_angle(pos[0], pos[1]):.1f}°）")

            elif key == "c":
                pos = show_pos()
                if pos:
                    y_center = pos[1]
                    print(f"  [记录] 中心 y = {y_center:.2f} cm  "
                          f"（底座角={base_angle(pos[0], pos[1]):.1f}°）")

            elif key == "ri":
                pos = show_pos()
                if pos:
                    y_right = pos[1]
                    print(f"  [记录] 右极限 y = {y_right:.2f} cm  "
                          f"（底座角={base_angle(pos[0], pos[1]):.1f}°）")

            else:
                print("  未知按键：u / r / p / l / c / ri / q")

    finally:
        arm.shutdown()

    # ── 打印最终参数 ──────────────────────────────────────────
    print()
    print("=" * 58)
    print("  示教完毕 —— 将以下参数填入 face_track.py")
    print("=" * 58)

    if y_left is None and y_right is None and y_center is None:
        print("  未记录任何数据")
    else:
        lv = f"{y_left:.2f}"  if y_left   is not None else "未记录"
        rv = f"{y_right:.2f}" if y_right  is not None else "未记录"
        cv = f"{y_center:.2f}" if y_center is not None else "0.00"

        print(f"  Y_MIN    = {lv}   # 左极限")
        print(f"  Y_MAX    = {rv}   # 右极限")
        print(f"  Y_CENTER = {cv}   # 中心")
        print(f"  TRACK_X  = {TEACH_X}")
        print(f"  TRACK_Z  = {TEACH_Z}")
        print(f"  TRACK_PITCH = {TEACH_PITCH}")

        if y_left is not None and y_right is not None:
            span = abs(y_right - y_left)
            print(f"\n  底座 y 总跨度: {span:.2f} cm")

    print("=" * 58)


if __name__ == "__main__":
    main()
