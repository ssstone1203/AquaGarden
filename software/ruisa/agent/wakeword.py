"""
wakeword.py  ——  唤醒词检测（基于 openWakeWord，独立模块）
依赖：pip install openwakeword pyaudio numpy
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


SAMPLE_RATE  = 16000
CHUNK_SIZE   = 1280
AUDIO_FORMAT = pyaudio.paInt16
CHANNELS     = 1


class WakeWordDetector:
    """
    后台持续监听麦克风，检测到唤醒词时调用 on_wake(model_name, score)。

    参数
    ----
    model      : 内置模型名（如 "hey_jarvis"）或自定义 .onnx 的完整路径
    threshold  : 置信度阈值，默认 0.5
    cooldown   : 触发后冷却秒数，防止连续触发
    on_wake    : 回调函数，签名 on_wake(model_name: str, score: float)
    """

    def __init__(
        self,
        model:     str           = "hey_jarvis",
        threshold: float         = 0.5,
        cooldown:  float         = 2.0,
        on_wake:   Optional[Callable] = None,
    ):
        self.threshold  = threshold
        self.cooldown   = cooldown
        self.on_wake    = on_wake or self._default_callback
        self._running   = False
        self._last_wake = 0.0

        if os.path.isfile(model):
            self._model = Model(wakeword_models=[model], inference_framework="onnx")
        else:
            models_dir = Path(openwakeword.__file__).parent / "resources" / "models"
            if not (models_dir / f"{model}_v0.1.onnx").exists():
                print(f"[唤醒] 下载内置模型 {model}...")
                openwakeword.utils.download_models()
            self._model = Model(wakeword_models=[model], inference_framework="onnx")

        print(f"[唤醒] 已加载模型: {list(self._model.models.keys())}")

    @staticmethod
    def _default_callback(model_name: str, score: float):
        print(f"[唤醒] 触发！model={model_name}  score={score:.3f}")

    def run(self):
        """阻塞运行，Ctrl-C 停止"""
        self._running = True
        audio = pyaudio.PyAudio()
        device = self._find_input_device(audio)
        stream = audio.open(
            format=AUDIO_FORMAT, channels=CHANNELS,
            rate=SAMPLE_RATE, input=True,
            input_device_index=device,
            frames_per_buffer=CHUNK_SIZE,
        )
        print("[唤醒] 监听中...")
        try:
            while self._running:
                raw   = stream.read(CHUNK_SIZE, exception_on_overflow=False)
                chunk = np.frombuffer(raw, dtype=np.int16)
                for mdl, score in self._model.predict(chunk).items():
                    if score >= self.threshold:
                        now = time.time()
                        if now - self._last_wake >= self.cooldown:
                            self._last_wake = now
                            self.on_wake(mdl, float(score))
        except KeyboardInterrupt:
            pass
        finally:
            stream.stop_stream()
            stream.close()
            audio.terminate()
            self._running = False

    def start_background(self) -> threading.Thread:
        t = threading.Thread(target=self.run, daemon=True)
        t.start()
        return t

    def stop(self):
        self._running = False

    @staticmethod
    def _find_input_device(audio: pyaudio.PyAudio) -> Optional[int]:
        for i in range(audio.get_device_count()):
            info = audio.get_device_info_by_index(i)
            if info["maxInputChannels"] >= 1 and "usb" in info["name"].lower():
                return i
        return None
