"""
dialogue.py  ——  语音对话模块（阿里云 DashScope qwen3 系列）

功能：
1. 持续对话模式（唤醒一次后持续对话，直到用户说"拜拜"）
2. 更自然的语言表达，不体现"任务"字眼
3. 动作回放结合真实数据（如天气）
4. 优化的台灯控制语言支持

参考 xiaoshutong/src/agent/speech.py 的实现模式：
  - TTS: qwen3-tts-flash → 返回音频 URL → 下载 → pygame 播放
  - ASR: 录音到 WAV 文件 → qwen3-asr-flash → 返回文字
  - LLM: qwen-plus / qwen-turbo → 意图识别
"""

import time
import wave
import tempfile
import threading
import requests
import json as json_lib
import dashscope
from typing import Tuple, Optional, List, Generator

import config

# ── 依赖可用性检查 ──────────────────────────────────────────────────────
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


# ── 自然语言系统提示词 ──────────────────────────────────────────────────
_NATURAL_SYSTEM_PROMPT = """
你是一个可爱友好的学习助手机器人"小臂"，和小朋友聊天时要用自然的语言。

绝对规则：
1. 绝对不要说"执行任务"、"触发任务"、"开始任务"这样的词
2. 用"好的呀"、"我来帮你"、"让我想想"等自然开头
3. 回复要简短（50字以内）、口语化，适合语音播报
4. 可以用语气词和表情描述增加亲切感

任务对应的自然表达：
- 分拣积木："好的，我来帮你把积木按颜色分好类~"
- 台灯控制："好的，我帮你把灯调亮一点~" / "我把灯关掉啦"
- 人脸追踪："让我看看你有没有在认真学习和有没有坐好哦~"
- 题目解答："好的，让我来帮你看看这道题！"
- 打招呼："嗨！你好呀！很高兴见到你！"
- 跳舞："好的，我来跳个舞！"
- 点头："好的，我点头表示同意！"
- 摇头："好的，我摇摇头~"
- 看天气："让我抬头看看今天的天气~"

未识别意图时的回复：
- "嗯嗯，我听到了~ 你还有什么想让我帮忙的吗？"
- "好的好的，还有什么需要我帮忙的吗？"
"""

# ── 结束语关键词 ────────────────────────────────────────────────────────
_FAREWELL_WORDS = ["拜拜", "再见", "bye", "再见啦", "拜拜啦", "我走了", "不用了", "结束吧"]


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
    import os as _os

    try:
        dashscope.api_key = config.DASHSCOPE_API_KEY
        audio_url = f"file://{_os.path.abspath(wav_path)}"

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
    import os as _os

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
            _os.unlink(tmp)

    except Exception as e:
        print(f"[TTS] 失败: {e}")


def _chunk_for_tts(text: str, max_chars: int = 400) -> list:
    """
    将长文本切成多段，避免单次 TTS 过长失败；尽量在句号、问号等处断开。
    """
    text = text.strip()
    if not text:
        return []

    paragraphs = [p.strip() for p in text.splitlines() if p.strip()]
    flat       = "".join(p.replace("\n", " ").strip() for p in paragraphs)

    if len(flat) <= max_chars:
        return [flat]

    chunks   = []
    delims   = "。！？!?；;"
    i        = 0
    n        = len(flat)
    while i < n:
        end = min(i + max_chars, n)
        if end < n:
            cut = end
            for j in range(end - 1, i, -1):
                if flat[j] in delims:
                    cut = j + 1
                    break
            if cut <= i:
                cut = end
        else:
            cut = n
        piece = flat[i:cut].strip()
        if piece:
            chunks.append(piece)
        i = cut
    return chunks


def tts_only(text: str):
    """
    仅语音播报（不额外 print），用于已在别处完整打印过的长文本（如题目解答）。
    """
    if not text or not text.strip():
        return
    if not (config.DASHSCOPE_API_KEY and _PYGAME_OK):
        print("[TTS] 跳过语音：无 API Key 或 pygame 不可用")
        return
    for seg in _chunk_for_tts(text):
        _tts_play(seg)


# ================================================================
#  公共接口
# ================================================================

def speak(text: str):
    """播报文字（打印 + TTS；播放仅需 pygame，不依赖麦克风）"""
    print(f"[小臂] {text}")
    if not (config.DASHSCOPE_API_KEY and _PYGAME_OK):
        return
    _tts_play(text)


def listen() -> str:
    """
    录音并返回识别文字。
    无 pyaudio 时降级为键盘输入。
    """
    import os as _os

    if not (config.DASHSCOPE_API_KEY and _PYAUDIO_OK):
        return input("[键盘输入] > ").strip()

    wav = _record_wav()
    if not wav:
        return input("[键盘输入] > ").strip()

    try:
        text = _asr(wav)
    finally:
        try:
            _os.unlink(wav)
        except Exception:
            pass

    if text:
        print(f"[对话] 识别: {text}")
    else:
        print("[对话] 未识别到语音，请键盘输入")
        text = input("[键盘输入] > ").strip()

    return text


# ================================================================
#  自然语言响应生成
# ================================================================

def generate_natural_response(task_name: str, params: dict = None) -> str:
    """
    生成自然的回复（用于任务确认，不体现"任务"字眼）。
    """
    params = params or {}

    if task_name == "clamp":
        return "好的呀，我来帮你把积木按颜色分好类~"

    elif task_name == "led":
        preset = params.get("preset", "medium")
        presets = {
            "off":    "好的，我把灯关掉啦~",
            "low":    "好的，我帮你把灯光调暗一点，这样更护眼~",
            "medium": "好的，我把灯光调到刚刚好的亮度~",
            "high":   "好的，我把灯光调亮一些，这样更清晰~",
        }
        return presets.get(preset, "好的，我来帮你调灯光~")

    elif task_name == "face":
        return "好的，让我看看你有没有在认真学习和有没有坐好哦~"

    elif task_name == "answer":
        return "好的，让我来帮你看看这道题！"

    elif task_name == "action":
        action = params.get("action", "打招呼")
        # 勿在 dict 字面量里调用 get_weather_speak_text()：会无条件执行天气逻辑，
        # 且易与 Web/线程环境下的模块状态纠缠；仅「看天气」时再拉天气文案。
        if action == "看天气":
            return get_weather_speak_text()
        preset = {
            "打招呼": "好的，我来打个招呼！很高兴见到你~",
            "跳舞":   "好的，我来跳个舞！",
            "点头":   "好的，我点头表示同意！",
            "摇头":   "好的，我摇摇头~",
        }
        return preset.get(action, f"好的，我来做个{action}的动作~")

    else:
        return "嗯嗯，好的~"


def get_weather_speak_text() -> str:
    """获取天气播报文本（结合真实数据）"""
    weather_info = _get_weather_info()
    return (
        f"好的，让我抬头看看今天的天气~ "
        f"今天是{weather_info['date']}，{weather_info['weather']}，"
        f"气温{weather_info['temp']}度，{weather_info['tips']}"
    )


def _get_weather_info() -> dict:
    """
    获取天气信息。
    优先使用心知天气API，失败则使用模拟数据。
    """
    import os as _os

    try:
        # 尝试使用心知天气API获取真实天气
        xinzhi_key = _os.getenv("XINZHI_KEY") or _os.getenv("WEATHER_KEY")
        if xinzhi_key:
            try:
                import urllib.request
                import urllib.parse
                url = f"https://api.seniverse.com/v3/weather/now.json?key={xinzhi_key}&location=beijing&language=zh-Hans&unit=c"
                req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
                with urllib.request.urlopen(req, timeout=5) as resp:
                    data = json_lib.loads(resp.read().decode())
                    if data.get("results"):
                        now = data["results"][0]["now"]
                        return {
                            "date": "今天",
                            "weather": now.get("text", "晴朗"),
                            "temp": now.get("temperature", "25"),
                            "tips": _get_weather_tip(now.get("text", "")),
                        }
            except Exception:
                pass

        # 使用模拟数据
        import random
        weather_conditions = {
            "晴": ("晴天呢", ["阳光明媚，适合户外活动哦", "是个学习的好天气", "记得多喝水呀"]),
            "多云": ("多云天气", ["云朵飘呀飘的", "不会太晒也不会太暗", "挺舒服的天气"]),
            "阴": ("阴天", ["天有点阴沉沉的", "适合在室内学习", "注意休息眼睛哦"]),
            "雨": ("下雨啦", ["雨天不能出门啦", "正好在家看书学习", "记得关窗哦"]),
            "雪": ("下雪了呢", ["哇，雪花好漂亮", "可以堆个小雪人", "注意保暖哦"]),
        }

        conditions = list(weather_conditions.keys())
        condition = random.choice(conditions)
        text, tips = weather_conditions.get(condition, ("晴天", ["是个好日子"]))
        return {
            "date": "今天",
            "weather": text,
            "temp": str(random.randint(15, 32)),
            "tips": random.choice(tips),
        }
    except Exception:
        return {"date": "今天", "weather": "晴朗", "temp": "25", "tips": "是个学习的好天气"}


def _get_weather_tip(weather_text: str) -> str:
    """根据天气文字返回提示语"""
    tips = {
        "晴": ["阳光明媚，适合户外活动哦", "记得多喝水", "是个学习的好天气"],
        "多云": ["不会太晒也不会太暗", "挺舒服的天气", "云朵很可爱呢"],
        "阴": ["天有点阴沉", "适合在室内学习", "注意休息眼睛"],
        "雨": ["下雨啦不能出门", "正好在家看书", "记得关窗哦"],
        "雪": ["下雪了呢好漂亮", "可以看看窗外放松一下", "注意保暖哦"],
    }
    for key, tip_list in tips.items():
        if key in weather_text:
            import random
            return random.choice(tip_list)
    return "是个学习的好天气"


def should_end_session(text: str) -> bool:
    """判断是否应该结束会话"""
    if not text:
        return False
    return any(word in text for word in _FAREWELL_WORDS)


def generate_farewell_response() -> str:
    """生成告别语"""
    farewells = [
        "好的，下次见啦！有什么问题随时叫我哦~",
        "拜拜~ 好好休息，有需要再叫我！",
        "再见啦！祝你学习愉快！",
        "好的，我先去休息啦，有事再叫我~",
        "拜拜！希望我帮到你啦！",
    ]
    try:
        import random
        return random.choice(farewells)
    except Exception:
        return farewells[0]


# ================================================================
#  意图解析：文字 → (task_name, params)
# ================================================================

_SYSTEM_PROMPT_TASK = """
你是 Jarvis，一个辅助小朋友学习的 AI 机械臂助手。
根据用户说的话，判断要执行哪个任务，输出 JSON：
{"task": "<任务名>", "params": {}}

任务名只能是以下六个之一（前五个与 agent/agent.py 任务表一致）：
- "clamp"  : 颜色识别与分拣（params 可含 "color": "red"|"green"|"blue"）
- "led"    : 智能台灯（params 可含 "preset": "off"/"low"/"medium"/"high"）
- "face"   : 人脸识别追踪
- "answer" : 题目解答（params 可含 "question": "<具体问题>"）
- "action" : 动作执行（预录 JSON 关键帧回放；params 含 "action": "<动作名>"）
- "unknown": 无法判断

台灯 preset 规则：
- "off"    : 关闭台灯 / 熄灯 / 灯关掉 / 不开灯 / 关灯
- "low"    : 调暗 / 低亮 / 昏暗 / 暗一点
- "medium" : 正常亮度 / 中等 / 刚刚好
- "high"   : 调亮 / 高亮 / 最亮 / 亮一点 / 打开灯 / 开灯

动作 action 规则（action 值必须从以下名称中选一个）：
- "打招呼" : 打招呼 / 招手 / 挥手 / 你好 / hello
- "跳舞"   : 跳舞 / 跳个舞 / 舞蹈 / dance
- "点头"   : 点头 / 同意 / 好的
- "摇头"   : 摇头 / 不 / 拒绝
- "看天气" : 天气 / 看天气 / 抬头看

示例：
"帮我把积木分类"     → {"task": "clamp",  "params": {}}
"分拣红色积木"       → {"task": "clamp",  "params": {"color": "red"}}
"把绿色的弄过去"     → {"task": "clamp",  "params": {"color": "green"}}
"抓蓝色方块"         → {"task": "clamp",  "params": {"color": "blue"}}
"台灯调暗"       → {"task": "led",    "params": {"preset": "low"}}
"关闭台灯"       → {"task": "led",    "params": {"preset": "off"}}
"台灯调亮"       → {"task": "led",    "params": {"preset": "high"}}
"打开台灯"       → {"task": "led",    "params": {"preset": "high"}}
"看看我在不在"   → {"task": "face",   "params": {}}
"这道题怎么做"   → {"task": "answer", "params": {"question": "请解答图片中的题目"}}
"帮我分析这道题" → {"task": "answer", "params": {"question": "请分析并解答图片中的题目"}}
"跳个舞"         → {"task": "action", "params": {"action": "跳舞"}}
"打个招呼"       → {"task": "action", "params": {"action": "打招呼"}}
"点头表示同意"   → {"task": "action", "params": {"action": "点头"}}
"摇摇头"         → {"task": "action", "params": {"action": "摇头"}}
"看一下天气"     → {"task": "action", "params": {"action": "看天气"}}
"你好"           → {"task": "unknown","params": {}}

只输出 JSON，不要多余文字。
""".strip()

# 动作关键词 → 动作名（关键词匹配降级用）
_ACTION_KEYWORD_MAP = {
    kw: action
    for action, keywords in config.ACTION_KEYWORD_MAP.items()
    for kw in keywords
}

_KEYWORD_MAP = [
    (["颜色", "分拣", "积木", "物块", "夹取", "红", "绿", "蓝", "分类"], "clamp"),
    (["灯", "光", "亮度", "照明", "台灯", "打开灯", "关闭灯", "调亮", "调暗"], "led"),
    (["人脸", "追踪", "跟踪", "在不在", "看我", "脸", "座位"],            "face"),
    (["题目", "解答", "分析", "拍照", "拍题", "作业", "解题", "题", "怎么做"], "answer"),
    (list(_ACTION_KEYWORD_MAP.keys()),                                    "action"),
]


def _strip_json_fence(raw: str) -> str:
    """去掉模型常套的 ```json ... ``` 包裹，便于 json.loads。"""
    s = raw.strip()
    if not s.startswith("```"):
        return s
    lines = s.split("\n")
    if lines and lines[0].startswith("```"):
        lines = lines[1:]
    if lines and lines[-1].strip().startswith("```"):
        lines = lines[:-1]
    return "\n".join(lines).strip()


def parse(text: str) -> Tuple[str, dict]:
    """将用户话语映射到 (task_name, kwargs)"""
    if not text:
        return "unknown", {}

    if not config.DASHSCOPE_API_KEY:
        return _keyword_parse(text)

    try:
        from openai import OpenAI
        client = OpenAI(api_key=config.DASHSCOPE_API_KEY, base_url=config.DASHSCOPE_BASE_URL)
        resp   = client.chat.completions.create(
            model=config.LLM_MODEL,
            messages=[
                {"role": "system", "content": _SYSTEM_PROMPT_TASK},
                {"role": "user",   "content": text},
            ],
            temperature=0,
            max_tokens=128,
        )
        raw = resp.choices[0].message.content.strip()
        raw = _strip_json_fence(raw)
        try:
            data = json_lib.loads(raw)
        except json_lib.JSONDecodeError:
            print(f"[对话] LLM 返回非 JSON: {raw[:200]}…")
            return _keyword_parse(text)
        task = data.get("task", "unknown")
        params = data.get("params") or {}
        if not isinstance(params, dict):
            params = {}
        # 模型常把可操作指令判成 unknown：必须再跑关键词，否则不会触发台灯/分拣等
        if task == "unknown":
            return _keyword_parse(text)
        return task, params
    except Exception as e:
        print(f"[对话] LLM 解析失败: {e}，降级关键词匹配")
        return _keyword_parse(text)


def _keyword_parse(text: str) -> Tuple[str, dict]:
    for keywords, task in _KEYWORD_MAP:
        if any(w in text for w in keywords):
            if task == "led":
                return task, _parse_led_params(text)
            if task == "action":
                # 尝试从文本中直接推断动作名
                for kw, action_name in _ACTION_KEYWORD_MAP.items():
                    if kw in text:
                        return "action", {"action": action_name}
                return "action", {}
            return task, {}
    return "unknown", {}


def _parse_led_params(text: str) -> dict:
    """解析台灯参数"""
    preset = "medium"
    if any(w in text for w in ["关闭", "熄", "关掉", "关"]):
        preset = "off"
    elif any(w in text for w in ["调暗", "低", "暗", "暗一点"]):
        preset = "low"
    elif any(w in text for w in ["调亮", "高", "亮", "亮一点", "最亮", "打开", "开灯"]):
        preset = "high"
    return {"preset": preset}


# ================================================================
#  持续对话主循环
# ================================================================

def continuous_dialogue_loop() -> Generator[Tuple[str, dict], None, None]:
    """
    持续对话主循环（生成器）。
    唤醒一次后持续对话，直到用户说"拜拜"才退出。

    Yields:
        (task_name, params): 识别到的任务和参数

    使用示例:
        for task_name, params in continuous_dialogue_loop():
            if task_name == "clamp":
                task_clamp(arm)
            elif task_name == "led":
                task_led(arm, **params)
            # ...
    """
    print("=" * 56)
    print("  小臂 — AI 机械臂辅学助手  启动中...")
    print("=" * 56)

    # 打招呼
    speak("你好呀！我是小臂，你的学习小助手~ 有什么需要帮忙的吗？")

    # 持续对话循环
    while True:
        print("\n[小臂] 听你说...")
        text = listen()

        if not text:
            speak("嗯？我没听清楚，你可以再说一次吗？")
            continue

        # 检查是否结束
        if should_end_session(text):
            farewell = generate_farewell_response()
            speak(farewell)
            break

        # 解析意图
        task_name, params = parse(text)

        if task_name == "unknown":
            # 未能识别，用自然语言询问
            speak("嗯嗯，我听到了~ 你还有什么想让我帮忙的吗？")
        else:
            # 生成自然回复并说话
            response = generate_natural_response(task_name, params)
            speak(response)
            # 返回任务信息供外部执行
            yield task_name, params

    print("[小臂] 对话结束，等待下次唤醒...")


# ================================================================
#  旧接口兼容（保留以兼容旧代码）
# ================================================================

def ask_led_preset() -> str:
    """询问台灯亮度偏好"""
    speak("你想要什么亮度呢？关闭、低亮度、中等亮度还是高亮度？")
    text = listen()
    if any(w in text for w in ["关", "关闭", "熄", "灭", "off"]):
        return "off"
    if any(w in text for w in ["低", "暗", "low", "dim"]):
        return "low"
    if any(w in text for w in ["高", "亮", "high", "bright"]):
        return "high"
    return "medium"


def ask_question() -> str:
    """询问题目问题"""
    speak("好的，请把题目放在摄像头前，我来帮你看看！")
    text = listen()
    return text if text else "请解答图片中的题目"


def ask_action_name(available: list) -> str:
    """
    当 LLM/关键词未能识别出具体动作名时，朗读可用动作列表并让用户再说一次。
    返回匹配到的动作名，失败返回空字符串。
    """
    names_str = "、".join(available) if available else "无"
    speak(f"我可以做这些动作：{names_str}，你想看哪个呢？")
    text = listen()
    # 精确匹配
    for name in available:
        if name in text:
            return name
    # 关键词模糊匹配
    for kw, action_name in _ACTION_KEYWORD_MAP.items():
        if kw in text and action_name in available:
            return action_name
    return ""
