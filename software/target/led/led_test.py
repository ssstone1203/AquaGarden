#!/usr/bin/env python3
"""
led_test.py  —— 串口发送测试

用途：
  1 发送 "1\n"：固件当前把 "1" 用在 CALIB_POSES 递增（可能会触发手眼标定移动）
  0 发送 "0\n"：固件当前未显式处理（通常无回包）

你可以先验证串口通不通、固件是否返回内容。
后续如果要真正控制 RGB 亮度，需要在固件命令解析里加逻辑。
"""

import serial
import time

SERIAL_PORT = "COM5"
BAUDRATE = 115200


def main():
    print("=" * 50)
    print("LED Serial Test (send 1/0)")
    print(f"Port={SERIAL_PORT}  Baud={BAUDRATE}")
    print("输入：")
    print("  1  -> send '1\\n' (add brightness / or triggers CALIB in current firmware)")
    print("  0  -> send '0\\n' (may be ignored in current firmware)")
    print("  q  -> quit")
    print("=" * 50)

    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1.0)
    # 串口缓冲清空，避免读到旧回包
    ser.reset_input_buffer()
    time.sleep(0.2)

    try:
        while True:
            s = input("按键 > ").strip().lower()
            if s == "q":
                break

            if s not in ("0", "1"):
                print("[提示] 只能输入 0 / 1 / q")
                continue

            line = s + "\n"
            ser.write(line.encode("ascii", errors="ignore"))
            print(f"[发送] {s}")

            # 读一行回包（如果固件有输出）
            resp = ser.readline().decode(errors="ignore").strip()
            if resp:
                print(f"[回包] {resp}")
            else:
                print("[回包] (空，可能固件未处理该命令)")
    finally:
        ser.close()
        print("退出")


if __name__ == "__main__":
    main()

