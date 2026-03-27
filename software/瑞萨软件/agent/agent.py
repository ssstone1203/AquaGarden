#!/usr/bin/env python3
"""
agent.py  ——  AI 机械臂辅学系统主入口

工作流程：
  初始姿态 → 等待唤醒词 → 与 AI 对话确定任务 → 执行任务 → 回归初始姿态 → 循环

任务列表：
  1. clamp  —— 颜色识别与分拣
  2. led    —— 智能台灯
  3. face   —— 人脸识别追踪
  4. answer —— 题目解答

启动：
  python agent.py
"""

import sys
import os
import threading
import time

# agent 目录加入 path，确保所有本地模块可直接 import
sys.path.insert(0, os.path.dirname(__file__))

import config
import dialogue
from arm import Arm
from tasks import task_clamp, task_led, task_face, task_answer

try:
    from wakeword import WakeWordDetector
    _WAKE_AVAILABLE = True
except ImportError as _e:
    _WAKE_AVAILABLE = False
    print(f"[Agent] 唤醒词模块加载失败: {_e}")
    print("       请执行: pip install openwakeword pyaudio numpy")
    print("[Agent] 将使用 Enter 键模拟唤醒")


# ================================================================
#  任务分发表
# ================================================================

def _dispatch(arm: Arm, task_name: str, params: dict):
    """根据任务名调用对应任务，params 为来自 dialogue.parse() 的附加参数"""
    if task_name == "clamp":
        task_clamp(arm)

    elif task_name == "led":
        preset = params.get("preset") or dialogue.ask_led_preset()
        task_led(arm, preset_key=preset)

    elif task_name == "face":
        task_face(arm)

    elif task_name == "answer":
        question = params.get("question") or dialogue.ask_question()
        task_answer(arm, question=question)

    else:
        dialogue.speak("抱歉，没有理解你的意思，请再说一次。")


# ================================================================
#  主循环
# ================================================================

def main():
    print("=" * 56)
    print("  Jarvis — AI 机械臂辅学助手  启动中...")
    print("=" * 56)

    # 初始化串口
    try:
        arm = Arm(config.SERIAL_PORT)
    except Exception as e:
        print(f"[Agent] 串口连接失败: {e}")
        print(f"        请检查 config.py 中 SERIAL_PORT = '{config.SERIAL_PORT}'")
        sys.exit(1)

    if not arm.ping():
        print("[Agent] 警告：PING 无响应，请检查机械臂连接")

    # 移到初始姿态
    print("[Agent] 移到初始姿态...")
    arm.go_home()
    time.sleep(config.HOME["dur"] / 1000 + 0.5)
    dialogue.speak("你好！我是 Jarvis，你的学习小助手，随时叫我！")

    # ── 唤醒模式 ─────────────────────────────────────────────────
    _wake_event = threading.Event()

    def on_wake(model_name: str, score: float):
        print(f"\n[唤醒] 检测到唤醒词！({model_name}, score={score:.3f})")
        _wake_event.set()

    if _WAKE_AVAILABLE:
        detector = WakeWordDetector(
            model=config.WAKE_MODEL,
            threshold=config.WAKE_THRESHOLD,
            cooldown=config.WAKE_COOLDOWN,
            on_wake=on_wake,
        )
        detector.start_background()
        print(f"[Agent] 等待唤醒词 '{config.WAKE_MODEL}'...\n")
    else:
        print("[Agent] 按 Enter 键模拟唤醒，Ctrl-C 退出\n")

    # ── 主状态机循环 ──────────────────────────────────────────────
    try:
        while True:
            # 等待唤醒
            if _WAKE_AVAILABLE:
                _wake_event.wait()
                _wake_event.clear()
            else:
                input()   # Enter 键模拟唤醒
                print("[唤醒] 模拟唤醒触发")

            _TASK_LABELS = {
                "clamp":  "颜色识别与分拣",
                "led":    "智能台灯",
                "face":   "人脸识别追踪",
                "answer": "题目解答",
            }
            _TASK_HINT = "可选任务：分拣 / 台灯 / 人脸追踪 / 题目解答"

            # 语音识别 + 意图解析（最多重试 3 次）
            task_name, params = "unknown", {}
            for attempt in range(3):
                prompt = "我在，请告诉我要做什么任务？" if attempt == 0 else f"没听清，请再说一次。（{_TASK_HINT}）"
                dialogue.speak(prompt)
                text              = dialogue.listen()
                task_name, params = dialogue.parse(text)
                if task_name != "unknown":
                    break

            if task_name == "unknown":
                dialogue.speak("多次未能识别，退出等待下次唤醒。")
                arm.go_home()
                continue

            dialogue.speak(f"好的，开始执行：{_TASK_LABELS.get(task_name, task_name)}")

            # 执行任务
            try:
                _dispatch(arm, task_name, params)
            except KeyboardInterrupt:
                print("\n[Agent] 任务被中断")
            except Exception as e:
                print(f"[Agent] 任务异常: {e}")

            # 回归初始姿态
            print("[Agent] 任务完成，回归初始姿态...")
            arm.go_home()
            time.sleep(config.HOME["dur"] / 1000 + 0.3)

            if _WAKE_AVAILABLE:
                print(f"[Agent] 等待下次唤醒...\n")
            else:
                print("[Agent] 等待 Enter 键...\n")

    except KeyboardInterrupt:
        print("\n[Agent] 收到退出信号，关闭中...")
    finally:
        if _WAKE_AVAILABLE:
            detector.stop()
        arm.close()
        print("[Agent] 已退出")


if __name__ == "__main__":
    main()
