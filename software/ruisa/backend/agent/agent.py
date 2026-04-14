#!/usr/bin/env python3
"""
agent.py  ——  AI 机械臂辅学系统主入口

工作流程：
  初始姿态 → 等待唤醒词 → 持续对话直到用户说"拜拜" → 回归初始姿态 → 循环

任务列表：
  1. clamp  —— 颜色识别与分拣
  2. led    —— 智能台灯（支持语言调亮调暗打开关闭）
  3. face   —— 人脸识别追踪
  4. answer —— 题目解答
  5. action —— 动作执行

特点：
  - 持续对话模式：唤醒一次后持续对话，直到用户说"拜拜"才退出
  - 自然语言表达：不用"任务"等生硬词汇
  - 动作回放结合真实数据（如天气）
  - Debug 模式：键盘输入模拟唤醒和对话

启动：
  python agent.py
  # Debug 模式（不需要麦克风和语音）：
  DEBUG=1 python agent.py
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
from tasks import task_clamp, task_led, task_face, task_answer, task_action, list_actions

# Debug 模式：直接使用键盘输入，无需麦克风
DEBUG_MODE = os.getenv("DEBUG", "0") == "1"

try:
    from wakeword import WakeWordDetector
    _WAKE_AVAILABLE = True
except ImportError as _e:
    _WAKE_AVAILABLE = False
    print(f"[Agent] 唤醒词模块加载失败: {_e}")
    print("       请执行: pip install openwakeword pyaudio numpy")
    print("[Agent] 将使用 Enter 键模拟唤醒（DEBUG 模式）")


# ================================================================
#  任务分发表
# ================================================================

def _dispatch(arm: Arm, task_name: str, params: dict):
    """根据任务名调用对应任务，params 为来自 dialogue.parse() 的附加参数"""
    if task_name == "clamp":
        task_clamp(arm)

    elif task_name == "led":
        preset = params.get("preset") or "medium"
        task_led(arm, preset_key=preset)

    elif task_name == "face":
        task_face(arm)

    elif task_name == "answer":
        question = params.get("question") or dialogue.ask_question()
        task_answer(arm, question=question)

    elif task_name == "action":
        action_name = params.get("action", "")
        available   = list_actions()
        if action_name not in available:
            action_name = dialogue.ask_action_name(available)
        if action_name:
            task_action(arm, action_name)
        else:
            dialogue.speak("没有找到对应的动作，请重新尝试。")

    else:
        dialogue.speak("抱歉，没有理解你的意思，请再说一次。")


# ================================================================
#  持续对话主循环
# ================================================================

def _continuous_session(arm: Arm):
    """
    一次唤醒后的持续对话会话。
    用户说"拜拜"前会持续接收指令并执行任务。
    """
    # 打招呼
    dialogue.speak("你好呀！我是小臂，你的学习小助手~ 有什么需要帮忙的吗？")

    while True:
        print("\n[小臂] 听你说...")
        text = dialogue.listen()

        if not text:
            dialogue.speak("嗯？我没听清楚，你可以再说一次吗？")
            continue

        # 检查是否结束
        if dialogue.should_end_session(text):
            farewell = dialogue.generate_farewell_response()
            dialogue.speak(farewell)
            break

        # 解析意图
        task_name, params = dialogue.parse(text)

        if task_name == "unknown":
            dialogue.speak("嗯嗯，我听到了~ 你还有什么想让我帮忙的吗？")
        else:
            # 生成自然回复并说话
            response = dialogue.generate_natural_response(task_name, params)
            dialogue.speak(response)

            # 执行任务
            try:
                _dispatch(arm, task_name, params)
            except KeyboardInterrupt:
                print("\n[小臂] 任务被中断")
                dialogue.speak("好的，任务中断了。还想让我帮你做别的吗？")
            except Exception as e:
                print(f"[小臂] 任务异常: {e}")
                dialogue.speak("抱歉，遇到了一点问题，我们继续聊吧~")

    print("[小臂] 对话结束，等待下次唤醒...")


# ================================================================
#  主循环
# ================================================================

def main():
    print("=" * 56)
    print("  小臂 — AI 机械臂辅学助手  启动中...")
    print("=" * 56)
    if DEBUG_MODE:
        print("  [DEBUG 模式] 所有语音输入输出在终端进行")

    # 初始化串口
    try:
        arm = Arm()  # 串口：config.SERIAL_PORT / 环境变量 SERIAL_PORT（如 COM3）
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

    # ── 唤醒模式 ─────────────────────────────────────────────────
    _wake_event = threading.Event()

    def on_wake(model_name: str, score: float):
        print(f"\n[唤醒] 检测到唤醒词！({model_name}, score={score:.3f})")
        _wake_event.set()

    if _WAKE_AVAILABLE and not DEBUG_MODE:
        detector = WakeWordDetector(
            model=config.WAKE_MODEL,
            threshold=config.WAKE_THRESHOLD,
            cooldown=config.WAKE_COOLDOWN,
            on_wake=on_wake,
        )
        detector.start_background()
        print(f"[Agent] 等待唤醒词 '{config.WAKE_MODEL}'...\n")
    else:
        if not DEBUG_MODE:
            print("[Agent] 唤醒词不可用，使用 Enter 键模拟唤醒")
        print("[Agent] 按 Enter 键模拟唤醒，Ctrl-C 退出\n")

    # ── 主状态机循环 ──────────────────────────────────────────────
    try:
        while True:
            # 等待唤醒
            if _WAKE_AVAILABLE and not DEBUG_MODE:
                _wake_event.wait()
                _wake_event.clear()
            else:
                input()   # Enter 键模拟唤醒
                print("[唤醒] 模拟唤醒触发")

            # 持续对话会话（直到用户说"拜拜"）
            _continuous_session(arm)

            # 回归初始姿态
            print("[Agent] 回归初始姿态...")
            arm.go_home()
            time.sleep(config.HOME["dur"] / 1000 + 0.3)

            if _WAKE_AVAILABLE and not DEBUG_MODE:
                print(f"[Agent] 等待下次唤醒...\n")
            else:
                print("[Agent] 等待 Enter 键...\n")

    except KeyboardInterrupt:
        print("\n[Agent] 收到退出信号，关闭中...")
    finally:
        if _WAKE_AVAILABLE and not DEBUG_MODE:
            detector.stop()
        arm.close()
        print("[Agent] 已退出")


if __name__ == "__main__":
    main()
