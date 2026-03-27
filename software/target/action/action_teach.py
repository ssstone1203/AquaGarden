#!/usr/bin/env python3
"""
action_teach.py  ——  机械臂动作录制 & 回放工具
依赖：pip install pyserial

录制流程（五个动作依次录制）：
  初始化 → 机械臂归位
    ↓
  按 [u + Enter]  → 卸力 + 自动开始定时采样录制
    ↓
  自然地移动机械臂做出动作（系统每 200ms 自动采样一次）
    ↓
  按 [r]  → 停止采样 + 保存 + 归位 + 自动切换下一个动作
  按 [x]  → 放弃本条动作，重新录
    ↓
  重复以上步骤直到五个动作全部录完

回放命令（录制完成后或随时）：
  p <序号/名称>  ── 回放指定动作
  list           ── 列出已保存动作
  q              ── 退出
"""

import serial
import time
import sys
import os
import json
import threading
import msvcrt

# ================================================================
#  配置
# ================================================================
SERIAL_PORT      = "COM14"
SERIAL_BAUD      = 115200

SAMPLE_INTERVAL  = 0.2    # 录制采样间隔（秒），与 PLAYBACK_DUR_MS 匹配

# 录制后处理：去重 + 平滑
DEDUP_XYZ_THRESHOLD   = 0.5   # cm  - XYZ 位移小于此值视为未移动，合并帧
DEDUP_PITCH_THRESHOLD = 1.0   # deg - pitch 变化小于此值视为未移动，合并帧
SMOOTH_WINDOW         = 3     # 移动平均窗口大小（奇数，0 或 1 关闭）

# ── 回放时序参数（三者须满足：SERVO_DISPATCH < INTERVAL < DUR）────────────────
#
#  固件 Theta_To_Servo 串行派发 4 轴舵机耗时：4 × 30ms = 120ms（硬件固定）
#  MOVE_NB 从发出到收到 OK 的耗时 ≈ 120ms（即 SERVO_DISPATCH）
#
#  PLAYBACK_DUR_MS    每帧给舵机的运动时长，需 > SERVO_DISPATCH(120ms)
#                     舵机以此速度匀速插值到目标位置
#  PLAYBACK_INTERVAL_MS  相邻两帧的发送间隔，需满足：
#                        SERVO_DISPATCH(120) < INTERVAL < DUR
#                     overlap = DUR - INTERVAL = 250 - 170 = 80ms
#                     → 所有舵机（含最晚收到指令的第 4 轴）在新帧到达时
#                       仍有 80ms 的运动未完成，自然平滑衔接，不晃动不停顿
#  PAUSE_THRESHOLD_MS 录制时静止段经 dedup 合并后 duration 会变大，
#                     超过此阈值视为"有意停顿"，改用阻塞 MOVE 让舵机到位稳定后
#                     再继续，避免停顿段被 MOVE_NB 连续打断
PLAYBACK_DUR_MS      = 250   # ms（建议值：200～400）
PLAYBACK_INTERVAL_MS = 170   # ms（建议值：DUR - 60 ～ DUR - 100）
PAUSE_THRESHOLD_MS   = 350   # ms（建议值：SAMPLE_INTERVAL*1000 * 1.5）

_HERE       = os.path.dirname(os.path.abspath(__file__))
ACTIONS_DIR = os.path.join(_HERE, "actions")

PRESET_ACTIONS = [
    "跳舞",
    "打招呼",
    "点头",
    "看天气",
    "摇头",
]


# ================================================================
#  串口控制（带线程锁，采样线程与主线程共用）
# ================================================================
class Arm:
    def __init__(self, port, baud=115200):
        self._s   = serial.Serial(port, baud, timeout=0)
        self._lock = threading.Lock()
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
        with self._lock:
            self._s.write((c + "\n").encode())
            return self._readline(timeout)

    def ping(self):
        return self.cmd("PING", timeout=2) == "PONG"

    def move(self, x, y, z, pitch, dur=1200):
        r = self.cmd(
            f"MOVE {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} -90 90 {int(dur)}",
            timeout=dur / 1000 + 5,
        )
        return r == "OK"

    def move_nb(self, x, y, z, pitch, dur=250):
        """纯非阻塞 MOVE_NB：固件串行派发完 4 轴舵机后立即回 OK（约 120ms）。
        不含任何等待——调用方 playback_action 按 PLAYBACK_INTERVAL_MS 节拍
        控制下一帧的发送时机，实现丝滑连续运动。"""
        r = self.cmd(
            f"MOVE_NB {x:.2f} {y:.2f} {z:.2f} {pitch:.1f} -90 90 {int(dur)}",
            timeout=3,
        )
        return r == "OK"

    def read_pos(self):
        r = self.cmd("READ_POS", timeout=2)
        try:
            vals = [float(v) for v in r.split(",")]
            if len(vals) == 4:
                return tuple(vals)
        except Exception:
            pass
        return None

    def unload(self):
        return self.cmd("UNLOAD", timeout=3) == "OK"

    def reset(self):
        return self.cmd("RESET", timeout=5)

    def shutdown(self):
        self._s.close()


# ================================================================
#  录制后处理：去重 + 平滑
# ================================================================
def smooth_keyframes(keyframes: list) -> list:
    """
    减少回放卡顿：
    1. 合并连续的近似重复帧（累加 duration），将静止段压缩成一次长停顿
    2. 对保留帧做移动平均，平滑轨迹噪声
    """
    if len(keyframes) < 2:
        return keyframes

    # 阶段1：合并近似重复帧
    merged = [dict(keyframes[0])]
    for kf in keyframes[1:]:
        prev = merged[-1]
        dist_xyz = (
            (kf["x"]     - prev["x"])     ** 2 +
            (kf["y"]     - prev["y"])     ** 2 +
            (kf["z"]     - prev["z"])     ** 2
        ) ** 0.5
        dist_p = abs(kf["pitch"] - prev["pitch"])
        if dist_xyz < DEDUP_XYZ_THRESHOLD and dist_p < DEDUP_PITCH_THRESHOLD:
            # 位置几乎未变 → 累加停留时间，不增加新帧
            merged[-1]["duration"] += kf["duration"]
        else:
            merged.append(dict(kf))

    # 阶段2：移动平均平滑（不改变 duration）
    if SMOOTH_WINDOW >= 2 and len(merged) >= SMOOTH_WINDOW:
        half = SMOOTH_WINDOW // 2
        smoothed = []
        for i, kf in enumerate(merged):
            lo = max(0, i - half)
            hi = min(len(merged) - 1, i + half)
            w  = merged[lo : hi + 1]
            n  = len(w)
            smoothed.append({
                "x":        round(sum(f["x"]     for f in w) / n, 2),
                "y":        round(sum(f["y"]     for f in w) / n, 2),
                "z":        round(sum(f["z"]     for f in w) / n, 2),
                "pitch":    round(sum(f["pitch"] for f in w) / n, 1),
                "duration": kf["duration"],
            })
        merged = smoothed

    return merged


# ================================================================
#  动作文件 I/O
# ================================================================
def action_path(name: str) -> str:
    safe = name.replace("/", "_").replace("\\", "_")
    return os.path.join(ACTIONS_DIR, f"{safe}.json")


def save_action(name: str, keyframes: list, verbose: bool = True):
    os.makedirs(ACTIONS_DIR, exist_ok=True)
    raw_count   = len(keyframes)
    keyframes   = smooth_keyframes(keyframes)
    smooth_count = len(keyframes)
    if verbose and raw_count != smooth_count:
        print(f"  [平滑] {raw_count} 帧 → {smooth_count} 帧"
              f"（去除 {raw_count - smooth_count} 个冗余帧，窗口={SMOOTH_WINDOW}）")
    data = {"name": name, "keyframes": keyframes}
    path = action_path(name)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    return path


def load_action(name: str):
    path = action_path(name)
    if not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data.get("keyframes", [])


def list_saved_actions():
    os.makedirs(ACTIONS_DIR, exist_ok=True)
    files = [f for f in os.listdir(ACTIONS_DIR) if f.endswith(".json")]
    return [os.path.splitext(f)[0] for f in sorted(files)]


# ================================================================
#  后台采样线程
# ================================================================
def _sampler(arm: Arm, keyframes: list, stop_evt: threading.Event, interval: float):
    """每隔 interval 秒读一次坐标，追加到 keyframes 列表"""
    while not stop_evt.is_set():
        pos = arm.read_pos()
        if pos:
            x, y, z, pitch = pos
            keyframes.append({
                "x":        round(x,     2),
                "y":        round(y,     2),
                "z":        round(z,     2),
                "pitch":    round(pitch, 1),
                "duration": int(interval * 1000),
            })
        stop_evt.wait(interval)


def start_sampler(arm: Arm, keyframes: list, interval: float):
    stop_evt = threading.Event()
    t = threading.Thread(
        target=_sampler,
        args=(arm, keyframes, stop_evt, interval),
        daemon=True,
    )
    t.start()
    return stop_evt, t


def stop_sampler(stop_evt: threading.Event, t: threading.Thread):
    stop_evt.set()
    t.join(timeout=3)


# ================================================================
#  回放
# ================================================================
def playback_action(arm: Arm, name: str):
    keyframes = load_action(name)
    if keyframes is None:
        print(f"  [错误] 找不到动作：{name}")
        return False
    if not keyframes:
        print("  [错误] 动作文件中没有关键帧")
        return False

    # 估算总时长：运动帧按 PLAYBACK_INTERVAL_MS，停顿帧按存储时长
    est_ms = sum(
        frame.get("duration", 200) if frame.get("duration", 200) > PAUSE_THRESHOLD_MS
        else PLAYBACK_INTERVAL_MS
        for frame in keyframes
    )
    print(f"\n{'='*54}")
    print(f"  回放：【{name}】  共 {len(keyframes)} 帧  估计时长 {est_ms/1000:.1f}s")
    print(f"  DUR={PLAYBACK_DUR_MS}ms  INTERVAL={PLAYBACK_INTERVAL_MS}ms  "
          f"OVERLAP={PLAYBACK_DUR_MS - PLAYBACK_INTERVAL_MS}ms")
    print(f"{'='*54}")
    print("[准备] 归位中...")
    arm.reset()
    time.sleep(2.0)

    total = len(keyframes)
    for i, frame in enumerate(keyframes, 1):
        x, y, z, pitch = frame["x"], frame["y"], frame["z"], frame["pitch"]
        stored_dur = frame.get("duration", 200)
        is_last    = (i == total)
        is_pause   = stored_dur > PAUSE_THRESHOLD_MS

        print(f"\r  [{i:3d}/{total}] x={x:6.2f}  y={y:6.2f}  "
              f"z={z:6.2f}  pitch={pitch:5.1f}°  "
              f"{'[停顿]' if is_pause else '[运动]'}",
              end="", flush=True)

        t0 = time.perf_counter()

        if is_last or is_pause:
            # 末帧 / 有意停顿：阻塞 MOVE，舵机到位稳定后再继续
            # stored_dur 记录了原始停顿时长，原封不动传给固件
            eff_dur = max(stored_dur, PLAYBACK_DUR_MS)
            arm.move(x, y, z, pitch, dur=eff_dur)
        else:
            # 运动帧：MOVE_NB（约 120ms 后收到 OK），精确按 PLAYBACK_INTERVAL_MS 节拍推进
            # 保证所有舵机在新帧到达时仍有 overlap=(DUR-INTERVAL) ms 的运动未完成
            arm.move_nb(x, y, z, pitch, dur=PLAYBACK_DUR_MS)
            elapsed_ms = (time.perf_counter() - t0) * 1000.0
            wait_ms = PLAYBACK_INTERVAL_MS - elapsed_ms
            if wait_ms > 0:
                time.sleep(wait_ms / 1000.0)

    print(f"\n[完成] 【{name}】 回放结束\n")
    return True


# ================================================================
#  录制主循环（流水线 + 自动采样）
# ================================================================
def _wait_key():
    """阻塞直到用户按下单个字符键，返回小写字符（不需要回车）"""
    while True:
        if msvcrt.kbhit():
            ch = msvcrt.getwch()
            if ord(ch) in (3, 26):   # Ctrl-C / Ctrl-Z → 当作 q
                return 'q'
            return ch.lower()
        time.sleep(0.05)


def record_all(arm: Arm):
    action_idx = 0
    total      = len(PRESET_ACTIONS)

    print(f"\n{'='*54}")
    print(f"  流水线自动采样录制  ──  共 {total} 个动作")
    print(f"  采样间隔：{int(SAMPLE_INTERVAL*1000)} ms")
    print(f"{'='*54}")
    print("[初始化] 归位中...")
    arm.reset()
    time.sleep(2.0)
    print("[OK] 归位完成\n")

    while action_idx < total:
        name = PRESET_ACTIONS[action_idx]

        # ── 等待开始录制 ───────────────────────────────────────
        print(f"┌─ [{action_idx+1}/{total}] 【{name}】 ─────────────────────┐")
        print(f"│  u + Enter → 卸力并开始录制                          │")
        print(f"│  s + Enter → 跳过本条动作                            │")
        print(f"│  q + Enter → 退出录制模式                            │")
        print(f"└──────────────────────────────────────────────────────┘")

        cmd = input("  > ").strip().lower()

        if cmd == "q":
            print("退出录制模式")
            return

        elif cmd == "s":
            print(f"  跳过【{name}】")
            action_idx += 1
            if action_idx < total:
                print("[归位] 切换下一个动作...\n")
                arm.reset()
                time.sleep(2.0)
            continue

        elif cmd != "u":
            print("  可用：u（开始录制）/ s（跳过）/ q（退出）")
            continue

        # ── 卸力 + 启动采样 ────────────────────────────────────
        if not arm.unload():
            print("  [错误] 卸力失败，请重试")
            continue

        keyframes  = []
        stop_evt, sampler_t = start_sampler(arm, keyframes, SAMPLE_INTERVAL)

        print(f"\n  ● 录制中【{name}】── 自然移动机械臂，做完后按键：")
        print(f"    r → 停止录制并保存")
        print(f"    x → 放弃本条动作，重新录\n")

        # ── 录制中，等待 r / x ─────────────────────────────────
        last_show  = time.time()
        result_key = None

        while True:
            now = time.time()
            if now - last_show >= 0.5:
                print(f"\r  ● 已采样 {len(keyframes):4d} 帧"
                      f"  ({len(keyframes)*SAMPLE_INTERVAL:.1f}s)"
                      f"  按 [r] 保存  [x] 重录",
                      end="", flush=True)
                last_show = now

            if msvcrt.kbhit():
                ch = msvcrt.getwch().lower()
                if ch in ('r', 'x', 'q'):
                    result_key = ch
                    break

            time.sleep(0.05)

        print()  # 换行
        stop_sampler(stop_evt, sampler_t)

        if result_key == 'q':
            print("退出录制模式")
            arm.reset()
            return

        elif result_key == 'x':
            print(f"  [放弃] 丢弃 {len(keyframes)} 帧，重新录制【{name}】")
            print("[归位] 归位中...")
            arm.reset()
            time.sleep(2.0)
            print("[OK] 归位完成，再次按 [u] 开始录制\n")
            continue

        else:  # r → 保存
            if not keyframes:
                print("  [提示] 未采到任何数据，请重试")
                arm.reset()
                time.sleep(2.0)
                continue

            path = save_action(name, keyframes)
            dur_total = len(keyframes) * SAMPLE_INTERVAL
            print(f"  [保存] 【{name}】 {len(keyframes)} 帧  "
                  f"时长 {dur_total:.1f}s → {path}")

            action_idx += 1
            if action_idx < total:
                print("[归位] 归位中，准备下一个动作...")
                arm.reset()
                time.sleep(2.0)
                print("[OK] 归位完成\n")
            else:
                print("\n[完成] 所有动作录制完毕！")
                arm.reset()


# ================================================================
#  主菜单
# ================================================================
def print_menu():
    print()
    print("╔══════════════════════════════════════════════════════╗")
    print("║          机械臂动作录制 & 回放工具                   ║")
    print("╠══════════════════════════════════════════════════════╣")
    for i, name in enumerate(PRESET_ACTIONS, 1):
        kf   = load_action(name)
        flag = f"{len(kf)}帧/{len(kf)*SAMPLE_INTERVAL:.1f}s" if kf else "未录制"
        line = f"║  {i}. {name}  [{flag}]"
        print(line + " " * max(0, 54 - len(line)) + "║")
    print("╠══════════════════════════════════════════════════════╣")
    print("║  rec              ── 流水线录制（依次录制所有动作）  ║")
    print("║  p <序号/名称>     ── 回放动作（示例：p 1 / p 跳舞） ║")
    print("║  smooth <名称/all> ── 重新平滑已有动作文件           ║")
    print("║  list             ── 列出已保存动作                  ║")
    print("║  q                ── 退出                            ║")
    print("╚══════════════════════════════════════════════════════╝")


def resolve_name(token: str) -> str:
    token = token.strip()
    if token.isdigit():
        idx = int(token) - 1
        if 0 <= idx < len(PRESET_ACTIONS):
            return PRESET_ACTIONS[idx]
    return token


def main():
    print_menu()

    print(f"\n[连接] 串口 {SERIAL_PORT}...")
    try:
        arm = Arm(SERIAL_PORT, SERIAL_BAUD)
    except serial.SerialException as e:
        sys.exit(f"[错误] 串口连接失败: {e}")

    if arm.ping():
        print(f"[OK] 已连接\n")
    else:
        print(f"[警告] 未收到 PONG，请确认固件处于 PC_CONTROL 模式\n")

    try:
        while True:
            try:
                raw = input("命令 > ").strip()
            except (EOFError, KeyboardInterrupt):
                print("\n退出")
                break

            if not raw:
                continue

            parts = raw.split(None, 1)
            cmd   = parts[0].lower()
            arg   = parts[1].strip() if len(parts) > 1 else ""

            if cmd == "q":
                print("退出")
                break

            elif cmd == "rec":
                record_all(arm)
                print_menu()

            elif cmd == "p":
                if not arg:
                    print("  用法：p <序号或名称>  示例：p 1 / p 打招呼")
                    continue
                name = resolve_name(arg)
                playback_action(arm, name)

            elif cmd == "smooth":
                targets = list_saved_actions() if arg.lower() in ("all", "") else [resolve_name(arg)]
                for name in targets:
                    kf = load_action(name)
                    if kf is None:
                        print(f"  [跳过] 找不到动作：{name}")
                        continue
                    path = save_action(name, kf, verbose=False)
                    raw  = len(kf)
                    new  = len(load_action(name))
                    print(f"  [平滑] 【{name}】 {raw} 帧 → {new} 帧  → {path}")
                print()

            elif cmd == "list":
                saved = list_saved_actions()
                if saved:
                    print(f"\n  已保存 {len(saved)} 个动作：")
                    for n in saved:
                        kf  = load_action(n)
                        cnt = len(kf) if kf else 0
                        dur = cnt * SAMPLE_INTERVAL
                        mark = "✓" if n in PRESET_ACTIONS else " "
                        print(f"    [{mark}] {n}  ({cnt} 帧 / {dur:.1f}s)")
                else:
                    print("  暂无已保存的动作")
                print()

            else:
                print("  未知命令，可用：rec / p / smooth / list / q")

    finally:
        arm.shutdown()
        print("[退出] 串口已关闭")


if __name__ == "__main__":
    main()
