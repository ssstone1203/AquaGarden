"""
dialogue.py  ——  唤醒后与用户的语音对话
职责：
  1. listen()  —— 录音并转文字（STT）
  2. parse()   —— 用 LLM 将文字映射到任务名和附加参数
  3. speak()   —— 简单打印提示（可替换为 TTS）
"""

import time
from typing import Tuple, Optional
from openai import OpenAI

import config

# ── 尝试导入语音识别库（可选） ────────────────────────────────────
try:
    import speech_recognition as sr
    _SR_AVAILABLE = True
except ImportError:
    _SR_AVAILABLE = False
    print("[对话] 提示：未安装 speech_recognition，将使用键盘输入模式")
    print("       可执行：pip install SpeechRecognition pyaudio")


# ================================================================
#  语音转文字
# ================================================================

def listen(prompt: str = "请说出要执行的任务...") -> str:
    """
    录音并返回识别文字。
    若 speech_recognition 不可用，则回退到键盘输入。
    """
    speak(prompt)

    if not _SR_AVAILABLE:
        return input("[键盘输入] > ").strip()

    recognizer = sr.Recognizer()
    with sr.Microphone() as source:
        recognizer.adjust_for_ambient_noise(source, duration=0.5)
        print(f"[对话] 正在聆听...（最多 {config.STT_PHRASE_LIMIT}s）")
        try:
            audio = recognizer.listen(
                source,
                timeout=config.STT_TIMEOUT,
                phrase_time_limit=config.STT_PHRASE_LIMIT,
            )
        except sr.WaitTimeoutError:
            print("[对话] 未检测到说话，超时")
            return ""

    try:
        text = recognizer.recognize_google(audio, language=config.STT_LANGUAGE)
        print(f"[对话] 识别: {text}")
        return text
    except sr.UnknownValueError:
        print("[对话] 无法识别语音")
        return ""
    except sr.RequestError as e:
        print(f"[对话] STT 请求失败: {e}，切换到键盘输入")
        return input("[键盘输入] > ").strip()


# ================================================================
#  文字提示（可替换为 TTS）
# ================================================================

def speak(text: str):
    print(f"[机械臂] {text}")


# ================================================================
#  意图解析：用户话语 → (task_name, kwargs)
# ================================================================

_SYSTEM_PROMPT = """
你是机械臂助手的意图分析器。根据用户说的话，输出 JSON，格式如下：
{"task": "<任务名>", "params": {}}

任务名只能是以下四个之一：
- "clamp"  : 颜色识别与分拣
- "led"    : 智能台灯（params 中可含 "preset": "low"/"medium"/"high"）
- "face"   : 人脸识别追踪
- "answer" : 题目解答（params 中可含 "question": "<用户问题>"）
- "unknown": 无法判断

示例：
用户: "帮我把红色积木分开" → {"task": "clamp", "params": {}}
用户: "台灯调暗一点"      → {"task": "led",   "params": {"preset": "low"}}
用户: "追踪我的脸"        → {"task": "face",  "params": {}}
用户: "帮我解这道数学题"  → {"task": "answer","params": {"question": "请解答图片中的数学题"}}
用户: "你好"              → {"task": "unknown","params": {}}

只输出 JSON，不要多余文字。
""".strip()

# 关键词降级映射（无 API Key 时使用）
_KEYWORD_MAP = [
    (["颜色", "分拣", "积木", "物块", "夹取", "红", "绿", "蓝"],  "clamp"),
    (["灯", "光", "亮度", "照明", "台灯"],                        "led"),
    (["人脸", "追踪", "跟踪", "检测人", "看我", "脸"],             "face"),
    (["题目", "解答", "拍照", "拍题", "作业", "解题", "题"],       "answer"),
]


def _keyword_parse(text: str) -> Tuple[str, dict]:
    for keywords, task in _KEYWORD_MAP:
        if any(w in text for w in keywords):
            return task, {}
    return "unknown", {}


def parse(text: str) -> Tuple[str, dict]:
    """
    将用户文字映射到 (task_name, kwargs)。
    - task_name: "clamp" / "led" / "face" / "answer" / "unknown"
    - kwargs: 传给任务函数的额外关键字参数（如 led 的 preset）
    """
    if not text:
        return "unknown", {}

    if not config.DASHSCOPE_API_KEY:
        return _keyword_parse(text)

    try:
        import json
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
        raw    = resp.choices[0].message.content.strip()
        data   = json.loads(raw)
        task   = data.get("task", "unknown")
        params = data.get("params", {})
        return task, params
    except Exception as e:
        print(f"[对话] LLM 解析失败: {e}，降级到关键词匹配")
        return _keyword_parse(text)


# ================================================================
#  台灯任务：在任务内部再问一次亮度
# ================================================================

def ask_led_preset() -> str:
    """询问并返回台灯亮度档位 low/medium/high"""
    speak("请说出亮度：低（low）、中（medium）还是高（high）？")
    text = listen(prompt="")
    text_low = text.lower()
    if any(w in text_low for w in ["低", "low", "暗", "dim"]):
        return "low"
    if any(w in text_low for w in ["高", "high", "bright", "亮", "最亮"]):
        return "high"
    return "medium"


# ================================================================
#  题目解答：在任务内部获取用户问题
# ================================================================

def ask_question() -> str:
    """询问用户想问什么"""
    speak("请说出你的问题，我来拍照解答。")
    text = listen(prompt="")
    return text if text else "请解答图片中的题目"
