#!/usr/bin/env python3
"""
awake.py  ——  唤醒词检测（基于 openWakeWord）
依赖：pip install openwakeword pyaudio numpy

功能：
  持续监听麦克风，检测到唤醒词后回调 on_wake()
  支持自定义唤醒词模型（.onnx 文件），也可使用内置预训练模型

用法：
  直接运行  : python awake.py
  作为模块  : from awake import WakeWordDetector
"""

import os
import time
import threading
from typing import Optional, Callable
from pathlib import Path
import numpy as np
import pyaudio
import openwakeword
from openwakeword.model import Model

# ================================================================
#  配置
# ================================================================

# 使用的唤醒词模型名（内置模型直接填名字，自定义 .onnx 填完整路径）
# 内置可选：hey_jarvis / alexa / hey_mycroft / timers 等
# 自定义示例：r"G:\AquaGarden\model\awake\xiao_yu.onnx"
WAKE_MODEL = "hey_jarvis"

# 触发阈值（0.0 ~ 1.0）：越高越不容易误触发，越低越灵敏
THRESHOLD = 0.5

# 触发后的冷却时间（秒），防止一次说话多次触发
COOLDOWN_SEC = 2.0

# 音频采集参数（openWakeWord 要求 16kHz / 16bit / mono）
SAMPLE_RATE   = 16000
CHUNK_SIZE    = 1280   # ~80ms per chunk
AUDIO_FORMAT  = pyaudio.paInt16
CHANNELS      = 1

# 触发提示音（系统 beep），设为 False 可关闭
BEEP_ON_WAKE  = True


# ================================================================
#  唤醒词检测器
# ================================================================
class WakeWordDetector:
    """
    持续监听麦克风，检测到唤醒词时调用回调函数。

    Parameters
    ----------
    model : str
        内置模型名（如 "hey_jarvis"）或自定义 .onnx 的完整路径
    threshold  : float
        置信度阈值，默认 0.5
    cooldown   : float
        触发后冷却秒数，默认 2.0
    on_wake    : callable | None
        唤醒回调，签名 on_wake(model_name: str, score: float)
    """

    def __init__(
        self,
        model:     str                    = WAKE_MODEL,
        threshold: float                  = THRESHOLD,
        cooldown:  float                  = COOLDOWN_SEC,
        on_wake:   Optional[Callable]     = None,
    ):
        self.threshold  = threshold
        self.cooldown   = cooldown
        self.on_wake    = on_wake or self._default_callback
        self._running   = False
        self._last_wake = 0.0

        # 加载模型：完整路径则为自定义模型，否则为内置模型名
        if os.path.isfile(model):
            print(f"[唤醒] 加载自定义模型: {model}")
            self._model = Model(
                wakeword_models=[model],
                inference_framework="onnx",
            )
        else:
            # 内置模型可能未下载，自动补充下载
            models_dir = Path(openwakeword.__file__).parent / "resources" / "models"
            model_file = models_dir / f"{model}_v0.1.onnx"
            if not model_file.exists():
                print(f"[唤醒] 内置模型 {model} 尚未下载，正在下载...")
                openwakeword.utils.download_models()
                print("[唤醒] 下载完成")
            print(f"[唤醒] 加载内置模型: {model}")
            self._model = Model(
                wakeword_models=[model],
                inference_framework="onnx",
            )

        self._model_names = list(self._model.models.keys())
        print(f"[唤醒] 已加载模型: {self._model_names}")

    # ── 默认回调（仅打印） ──────────────────────────────────────────
    @staticmethod
    def _default_callback(model_name: str, score: float):
        print(f"\n{'='*48}")
        print(f"  *** 唤醒词触发！***  模型={model_name}  置信度={score:.3f}")
        print(f"{'='*48}\n")
        if BEEP_ON_WAKE:
            import winsound
            winsound.Beep(1000, 200)

    # ── 启动检测（阻塞） ───────────────────────────────────────────
    def run(self):
        """阻塞式运行，Ctrl-C 停止"""
        self._running = True
        audio = pyaudio.PyAudio()

        # 自动选择第一个可用输入设备
        input_device = self._find_input_device(audio)
        device_name  = audio.get_device_info_by_index(input_device)["name"] if input_device is not None else "默认"
        print(f"[唤醒] 使用麦克风: {device_name}")
        print(f"[唤醒] 阈值={self.threshold}  冷却={self.cooldown}s")
        print("[唤醒] 监听中，等待唤醒词...（Ctrl-C 退出）\n")

        stream = audio.open(
            format=AUDIO_FORMAT,
            channels=CHANNELS,
            rate=SAMPLE_RATE,
            input=True,
            input_device_index=input_device,
            frames_per_buffer=CHUNK_SIZE,
        )

        try:
            while self._running:
                raw = stream.read(CHUNK_SIZE, exception_on_overflow=False)
                chunk = np.frombuffer(raw, dtype=np.int16)

                predictions = self._model.predict(chunk)

                for mdl, score in predictions.items():
                    if score >= self.threshold:
                        now = time.time()
                        if now - self._last_wake >= self.cooldown:
                            self._last_wake = now
                            self.on_wake(mdl, float(score))
        except KeyboardInterrupt:
            print("\n[唤醒] 已停止")
        finally:
            stream.stop_stream()
            stream.close()
            audio.terminate()
            self._running = False

    # ── 启动检测（后台线程） ───────────────────────────────────────
    def start_background(self):
        """在后台线程中运行，不阻塞主线程"""
        t = threading.Thread(target=self.run, daemon=True)
        t.start()
        return t

    def stop(self):
        self._running = False

    # ── 枚举输入设备，优先选 USB ──────────────────────────────────
    @staticmethod
    def _find_input_device(audio: pyaudio.PyAudio) -> Optional[int]:
        count = audio.get_device_count()
        usb_idx = None
        for i in range(count):
            info = audio.get_device_info_by_index(i)
            if info["maxInputChannels"] < 1:
                continue
            name = info["name"].lower()
            # 优先选 USB 麦克风
            if "usb" in name or "usbaudio" in name:
                usb_idx = i
                break
        return usb_idx  # None = pyaudio 默认设备


# ================================================================
#  列出音频设备（调试用）
# ================================================================
def list_audio_devices():
    audio = pyaudio.PyAudio()
    print("\n可用音频输入设备：")
    print(f"  {'ID':<4} {'名称'}")
    print(f"  {'-'*4} {'-'*40}")
    for i in range(audio.get_device_count()):
        info = audio.get_device_info_by_index(i)
        if info["maxInputChannels"] > 0:
            print(f"  {i:<4} {info['name']}")
    audio.terminate()
    print()


# ================================================================
#  直接运行示例
# ================================================================
if __name__ == "__main__":
    print("=" * 52)
    print("  唤醒词检测  awake.py  （openWakeWord）")
    print("=" * 52)

    list_audio_devices()

    detector = WakeWordDetector(
        model=WAKE_MODEL,
        threshold=THRESHOLD,
        cooldown=COOLDOWN_SEC,
    )
    detector.run()
