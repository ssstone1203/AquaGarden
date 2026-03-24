#!/usr/bin/env python3
"""
software/target/clamp/teach.py  ——  示教模式标定工具

用途：确定 clamp.py 所需的两个 z 参数
  · TABLE_Z    桌面高度（z，cm）
  · BLOCK_HEIGHT 已知为 3cm，BLOCK_TOP_Z = TABLE_Z + 3

操作流程：
  1. 烧录新固件后运行本脚本
  2. 脚本自动与机械臂建立连接，初始复位到安全位置
  3. 按 [U] 让所有舵机卸力，然后用手轻轻把机械臂末端移到目标位置
  4. 按 [T] 记录桌面 z   ← 把末端移到桌面上，然后按
  5. 按 [B] 记录物块顶面 z ← 把末端放到物块顶面上，然后按
  6. 按 [R] 重新上力（复位到初始位置）
  7. 按 [Q] 退出，脚本打印最终参数

依赖：pip install pyserial
"""

import serial
import time
import sys
import os

# ── 配置 ─────────────────────────────────────────────────────────────
SERIAL_PORT  = "COM5"
SERIAL_BAUD  = 115200
BLOCK_HEIGHT = 3.0          # cm，已知固定值

# ── 串口工具 ──────────────────────────────────────────────────────────
class ArmSerial:
    def __init__(self, port, baud):
        self._ser = serial.Serial(port, baud, timeout=0)
        time.sleep(0.3)
        self._ser.reset_input_buffer()
        self._buf = b""

    def _send(self, text):
        self._ser.write((text + "\n").encode())

    def _recv_line(self, timeout=6.0):
        t0 = time.time()
        while time.time() - t0 < timeout:
            if self._ser.in_waiting:
                self._buf += self._ser.read(self._ser.in_waiting)
            if b"\n" in self._buf:
                line, self._buf = self._buf.split(b"\n", 1)
                return line.decode(errors="ignore").strip()
            time.sleep(0.01)
        return ""

    def cmd(self, c, timeout=6.0):
        self._send(c)
        return self._recv_line(timeout)

    def close(self):
        self._ser.close()

# ── 读取当前末端坐标（正向运动学） ───────────────────────────────────
def read_pos(arm):
    """
    发送 READ_POS，解析回复 "x,y,z,pitch"
    返回 (x, y, z, pitch) 或 None
    """
    resp = arm.cmd("READ_POS", timeout=2.0)
    if not resp or resp == "ERR":
        return None
    try:
        vals = [float(v) for v in resp.split(",")]
        if len(vals) == 4:
            return tuple(vals)
    except ValueError:
        pass
    return None

# ── 主程序 ────────────────────────────────────────────────────────────
def main():
    print("=" * 58)
    print("  机械臂示教模式  ——  获取 TABLE_Z 参数")
    print("=" * 58)

    # 连接串口
    try:
        arm = ArmSerial(SERIAL_PORT, SERIAL_BAUD)
        print(f"[OK] 串口 {SERIAL_PORT} 已连接")
    except serial.SerialException as e:
        sys.exit(f"[错误] 无法打开串口 {SERIAL_PORT}: {e}")

    # 连通性测试
    if arm.cmd("PING", timeout=2.0) != "PONG":
        print("[警告] 未收到 PONG，请确认固件处于 PC_CONTROL 模式")

    # 先复位到安全位置
    print("[初始化] 机械臂复位中...")
    arm.cmd("RESET", timeout=3.0)
    time.sleep(1.8)
    print("[OK] 复位完成，末端在安全位置 (15, 0, 2)")
    print()

    # ── 显示操作菜单 ─────────────────────────────────────────────────
    print("操作按键（在此终端输入后回车）：")
    print()
    print("  [u]  卸力 —— 所有舵机松开，可用手自由移动机械臂")
    print("  [r]  上力 —— 机械臂复位到初始位置（重新受控）")
    print("  [p]  读坐标 —— 显示当前末端 x,y,z,pitch")
    print("  [t]  记录桌面 z  ← 先把末端放到桌面上再按")
    print("  [b]  记录物块顶面 z ← 先把末端放到物块顶面上再按")
    print("  [q]  退出并打印最终参数")
    print()
    print("-" * 58)

    table_z     = None
    block_top_z = None
    unloaded    = False

    try:
        while True:
            try:
                key = input("按键 > ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                break

            if key == "q":
                break

            elif key == "u":
                resp = arm.cmd("UNLOAD", timeout=2.0)
                if resp == "OK":
                    unloaded = True
                    print("  [OK] 舵机已卸力，现在可以用手移动机械臂")
                    print("       移动到目标位置后，按 [p] 确认坐标，按 [t]/[b] 记录")
                else:
                    print(f"  [错误] 卸力失败: {resp}")

            elif key == "r":
                print("  [复位] 机械臂正在复位（2000ms）...")
                arm.cmd("RESET", timeout=3.0)
                time.sleep(1.8)
                unloaded = False
                print("  [OK] 复位完成，舵机已重新上力")

            elif key == "p":
                pos = read_pos(arm)
                if pos:
                    x, y, z, pitch = pos
                    print(f"  当前坐标：x={x:.2f}  y={y:.2f}  z={z:.2f}  pitch={pitch:.1f}°")
                else:
                    print("  [错误] 读取失败（舵机是否已卸力无法响应？）")
                    print("         提示：卸力状态下舵机可能无法回报位置，请先小幅度移动再试")

            elif key == "t":
                pos = read_pos(arm)
                if pos:
                    x, y, z, pitch = pos
                    table_z = z
                    print(f"  [记录] 桌面 z = {table_z:.2f} cm  (x={x:.2f} y={y:.2f} pitch={pitch:.1f}°)")
                    # 自动推算物块顶面
                    block_top_z = table_z + BLOCK_HEIGHT
                    print(f"         物块顶面 z = {table_z:.2f} + {BLOCK_HEIGHT:.1f} = {block_top_z:.2f} cm")
                else:
                    print("  [错误] 读取坐标失败，请重试")

            elif key == "b":
                pos = read_pos(arm)
                if pos:
                    x, y, z, pitch = pos
                    block_top_z = z
                    # 反推桌面
                    table_z_inferred = block_top_z - BLOCK_HEIGHT
                    print(f"  [记录] 物块顶面 z = {block_top_z:.2f} cm  (x={x:.2f} y={y:.2f} pitch={pitch:.1f}°)")
                    print(f"         反推桌面 z = {block_top_z:.2f} - {BLOCK_HEIGHT:.1f} = {table_z_inferred:.2f} cm")
                    if table_z is None:
                        table_z = table_z_inferred
                        print(f"         已自动设置 TABLE_Z = {table_z:.2f} cm")
                else:
                    print("  [错误] 读取坐标失败，请重试")

            else:
                print("  未知按键，可用：u / r / p / t / b / q")

    finally:
        arm.close()

    # ── 打印最终参数 ──────────────────────────────────────────────────
    print()
    print("=" * 58)
    print("  示教结束，请将以下参数填入 clamp.py 配置区：")
    print("=" * 58)
    if table_z is not None:
        print(f"  TABLE_Z      = {table_z:.1f}   # 桌面 z（cm）")
        print(f"  BLOCK_HEIGHT =  3.0             # 物块高度（cm）")
        print()
        print("  → 物块顶面自动计算为 BLOCK_TOP_Z = TABLE_Z + BLOCK_HEIGHT")
        print(f"    = {table_z:.1f} + 3.0 = {table_z + BLOCK_HEIGHT:.1f} cm")
    else:
        print("  未记录任何数据，TABLE_Z 未更新")
    print("=" * 58)


if __name__ == "__main__":
    main()
