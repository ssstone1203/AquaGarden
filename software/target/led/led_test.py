#!/usr/bin/env python3
"""
led_test.py  —— 固定位置 + 固定灯色测试

启动后自动执行：
  1. 机械臂移动到固定位置（等待到位后继续）
  2. 设置亮度为半亮（128/255）
  3. 点亮灯环固定颜色 R=255 G=200 B=150
"""

import serial
import time

SERIAL_PORT = "COM14"
BAUDRATE    = 115200

LED_R       = 255
LED_G       = 200
LED_B       = 150
LED_BRIGHT  = 128       # 半亮度，范围 0-255

ARM_X       = 15.21
ARM_Y       = -1.28
ARM_Z       = 12.52
ARM_PITCH   = -27.4
ARM_MIN_P   = -90.0
ARM_MAX_P   = 90.0
ARM_DUR     = 2000      # 运动时长 ms，MOVE 命令会在固件侧阻塞等待


def send_cmd(ser: serial.Serial, cmd: str, timeout: float = 5.0) -> str:
    ser.write((cmd + "\n").encode("ascii", errors="ignore"))
    ser.timeout = timeout
    resp = ser.readline().decode(errors="ignore").strip()
    ser.timeout = 1.0
    return resp


def main():
    print("=" * 55)
    print("LED + 机械臂固定位置测试")
    print(f"  串口: {SERIAL_PORT}  波特率: {BAUDRATE}")
    print(f"  目标位置: x={ARM_X}  y={ARM_Y}  z={ARM_Z}  pitch={ARM_PITCH}°")
    print(f"  LED 颜色: R={LED_R}  G={LED_G}  B={LED_B}  亮度={LED_BRIGHT}/255")
    print("=" * 55)

    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1.0)
    ser.reset_input_buffer()
    time.sleep(0.2)

    try:
        # ── 步骤 1：移动机械臂 ──────────────────────────────────────────
        move_cmd = (
            f"MOVE {ARM_X} {ARM_Y} {ARM_Z} {ARM_PITCH} "
            f"{ARM_MIN_P} {ARM_MAX_P} {ARM_DUR}"
        )
        print(f"[1/3] 发送: {move_cmd}")
        resp = send_cmd(ser, move_cmd, timeout=ARM_DUR / 1000 + 3.0)
        if resp == "OK":
            print("      机械臂已到位")
        else:
            print("      警告：回包异常 (%s)，继续执行" % (resp or "无回包"))

        # ── 步骤 2：设置亮度 ────────────────────────────────────────────
        bright_cmd = f"LED_BRIGHT {LED_BRIGHT}"
        print(f"[2/3] 发送: {bright_cmd}")
        resp = send_cmd(ser, bright_cmd)
        print("      " + ("OK" if resp == "OK" else "警告：" + (resp or "无回包")))

        # ── 步骤 3：点亮灯环 ────────────────────────────────────────────
        led_cmd = f"LED_ALL {LED_R} {LED_G} {LED_B}"
        print(f"[3/3] 发送: {led_cmd}")
        resp = send_cmd(ser, led_cmd)
        print("      " + ("OK" if resp == "OK" else "警告：" + (resp or "无回包")))

        print("=" * 55)
        print("完成。按 Enter 退出（灯保持亮起）...")
        input()

    finally:
        ser.close()
        print("串口已关闭，退出")


if __name__ == "__main__":
    main()
