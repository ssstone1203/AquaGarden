"""
dialogue.py  ——  语音对话模块（阿里云 DashScope qwen3 系列）

参考 xiaoshutong/src/agent/speech.py 的实现模式：
  - TTS: qwen3-tts-flash → 返回音频 URL → 下载 → pygame 播放
  - ASR: 录音到 WAV 文件 → qwen3-asr-flash → 返回文字
  - LLM: qwen-plus / qwen-turbo → 意图识别
"""

import os
import time
import wave
import tempfile
import threading
import requests
import dashscope
from typing import Tuple, Optional

import config

# ── 依赖可用性检查（启动时打印一次） ──────────────────────────────
try:
    import pyaudio
    _PYAUDIO_OK = True
except ImportError:
    _PYAUDIO_OK = False
    print("[对话] ✗ pyaudio 未安装  →  pip install pyaudio")

try:
    import pygame
    _PYGAME_OK = True
except ImportError:
    _PYGAME_OK = False
    print("[对话] ✗ pygame 未安装  →  pip install pygame")

print(f"[对话] pyaudio={'✓' if _PYAUDIO_OK else '✗'}  pygame={'✓' if _PYGAME_OK else '✗'}")


# ================================================================
#  录音：麦克风 → WAV 文件
# ================================================================

def _record_wav(duration: int = None) -> Optional[str]:
    """
    录音到临时 WAV 文件，返回文件路径。
    duration=None 时使用 config.STT_PHRASE_LIMIT。
    """
    if not _PYAUDIO_OK:
        return None

    duration = duration or config.STT_PHRASE_LIMIT

    p = pyaudio.PyAudio()
    stream = p.open(
        format=pyaudio.paInt16, channels=1,
        rate=16000, input=True, frames_per_buffer=1024,
    )

    print(f"[对话] 录音中...（{duration}s，说完即可）")
    frames = []
    for _ in range(int(16000 / 1024 * duration)):
        frames.append(stream.read(1024, exception_on_overflow=False))

    stream.stop_stream()
    stream.close()
    p.terminate()

    tmp = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
    with wave.open(tmp.name, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(16000)
        wf.writeframes(b"".join(frames))

    return tmp.name


# ================================================================
#  ASR：WAV 文件 → 文字（qwen3-asr-flash）
# ================================================================

def _asr(wav_path: str) -> str:
    """调用 qwen3-asr-flash，返回识别文字"""
    try:
        dashscope.api_key = config.DASHSCOPE_API_KEY
        audio_url = f"file://{os.path.abspath(wav_path)}"

        response = dashscope.MultiModalConversation.call(
            api_key=config.DASHSCOPE_API_KEY,
            model=config.ASR_MODEL,
            messages=[
                {"role": "system", "content": [{"text": ""}]},
                {"role": "user",   "content": [{"audio": audio_url}]},
            ],
            result_format="message",
            asr_options={"enable_lid": True, "enable_itn": True, "language": "zh"},
        )

        if response.status_code == 200:
            choices = response.output.choices
            if choices:
                content = choices[0].message.content
                if content:
                    item = content[0]
                    text = (
                        item.get("text") if isinstance(item, dict)
                        else getattr(item, "text", None)
                    )
                    return text.strip() if text else ""
        else:
            print(f"[ASR] 错误 {response.status_code}: {response.message}")

    except Exception as e:
        print(f"[ASR] 失败: {e}")

    return ""


# ================================================================
#  TTS：文字 → 语音播放（qwen3-tts-flash → URL → pygame）
# ================================================================

def _tts_play(text: str):
    """调用 qwen3-tts-flash 合成语音，下载后用 pygame 播放"""
    try:
        dashscope.api_key = config.DASHSCOPE_API_KEY

        response = dashscope.MultiModalConversation.call(
            model=config.TTS_MODEL,
            api_key=config.DASHSCOPE_API_KEY,
            text=text,
            voice=config.TTS_VOICE,
            language_type="Chinese",
            stream=False,
        )

        if response.status_code != 200:
            print(f"[TTS] 错误 {response.status_code}: {response.message}")
            return

        audio_url  = response.output.audio.url
        audio_data = requests.get(audio_url, timeout=15).content

        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            f.write(audio_data)
            tmp = f.name

        try:
            pygame.mixer.init()
            pygame.mixer.music.load(tmp)
            pygame.mixer.music.play()
            while pygame.mixer.music.get_busy():
                pygame.time.wait(50)
            pygame.mixer.music.unload()
        finally:
            os.unlink(tmp)

    except Exception as e:
        print(f"[TTS] 失败: {e}")


# ================================================================
#  公共接口
# ================================================================

def speak(text: str):
    """播报文字（打印 + TTS）"""
    print(f"[Jarvis] {text}")
    if not (config.DASHSCOPE_API_KEY and _PYGAME_OK and _PYAUDIO_OK):
        return
    _tts_play(text)


def listen() -> str:
    """
    录音并返回识别文字。
    无 pyaudio 时降级为键盘输入。
    """
    if not (config.DASHSCOPE_API_KEY and _PYAUDIO_OK):
        return input("[键盘输入] > ").strip()

    wav = _record_wav()
    if not wav:
        return input("[键盘输入] > ").strip()

    try:
        text = _asr(wav)
    finally:
        try:
            os.unlink(wav)
        except Exception:
            pass

    if text:
        print(f"[对话] 识别: {text}")
    else:
        print("[对话] 未识别到语音，请键盘输入")
        text = input("[键盘输入] > ").strip()

    return text


# ================================================================
#  意图解析：文字 → (task_name, params)
# ================================================================

_SYSTEM_PROMPT = """
你是 Jarvis，一个辅助小朋友学习的 AI 机械臂助手。
根据用户说的话，判断要执行哪个任务，输出 JSON：
{"task": "<任务名>", "params": {}}

任务名只能是以下五个之一：
- "clamp"  : 颜色识别与分拣积木
- "led"    : 控制台灯（params 可含 "preset": "low"/"medium"/"high"）
- "face"   : 人脸识别追踪（检测小朋友是否在座位上）
- "answer" : 拍照解答题目（params 可含 "question": "<具体问题>"）
- "unknown": 无法判断

示例：
"帮我把积木分类" → {"task": "clamp", "params": {}}
"台灯调暗"       → {"task": "led",   "params": {"preset": "low"}}
"看看我在不在"   → {"task": "face",  "params": {}}
"这道题怎么做"   → {"task": "answer","params": {"question": "请解答图片中的题目"}}
"你好"           → {"task": "unknown","params": {}}

只输出 JSON，不要多余文字。
""".strip()

_KEYWORD_MAP = [
    (["颜色", "分拣", "积木", "物块", "夹取", "红", "绿", "蓝", "分类"], "clamp"),
    (["灯", "光", "亮度", "照明", "台灯"],                               "led"),
    (["人脸", "追踪", "跟踪", "在不在", "看我", "脸", "座位"],            "face"),
    (["题目", "解答", "拍照", "拍题", "作业", "解题", "题", "怎么做"],    "answer"),
]


def parse(text: str) -> Tuple[str, dict]:
    """将用户话语映射到 (task_name, kwargs)"""
    if not text:
        return "unknown", {}

    if not config.DASHSCOPE_API_KEY:
        return _keyword_parse(text)

    try:
        import json
        from openai import OpenAI
        client = OpenAI(api_key=config.DASHSCOPE_API_KEY, base_url=config.DASHSCOPE_BASE_URL)
        resp   = client.chat.completions.create(
            model=config.LLM_MODEL,
            messages=[
                {"role": "system", "content": _SYSTEM_PROMPT},
                {"role": "user",   "content": text},
            ],
            temperature=0,
            max_tokens=80,
        )
        data   = json.loads(resp.choices[0].message.content.strip())
        return data.get("task", "unknown"), data.get("params", {})
    except Exception as e:
        print(f"[对话] LLM 解析失败: {e}，降级关键词匹配")
        return _keyword_parse(text)


def _keyword_parse(text: str) -> Tuple[str, dict]:
    for keywords, task in _KEYWORD_MAP:
        if any(w in text for w in keywords):
            return task, {}
    return "unknown", {}


# ================================================================
#  台灯亮度询问
# ================================================================

def ask_led_preset() -> str:
    speak("请说出亮度：低、中还是高？")
    text = listen()
    if any(w in text for w in ["低", "暗", "low", "dim"]):
        return "low"
    if any(w in text for w in ["高", "亮", "high", "bright"]):
        return "high"
    return "medium"


# ================================================================
#  题目解答：获取用户问题
# ================================================================

def ask_question() -> str:
    speak("请告诉我题目的问题，我来帮你拍照解答。")
    text = listen()
    return text if text else "请解答图片中的题目"
