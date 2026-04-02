"""
config.py  ——  Agent 统一配置
修改此文件来调整所有硬件参数和 API 设置。
"""

import os
from pathlib import Path

# 加载 ruisa/.env、ruisa/backend/.env，使 SERIAL_PORT 等与 uvicorn 共用同一套环境变量
_RUISA_ROOT = Path(__file__).resolve().parent.parent
try:
    from dotenv import load_dotenv

    load_dotenv(_RUISA_ROOT / ".env")
    load_dotenv(_RUISA_ROOT / "backend" / ".env")
except ImportError:
    pass


def _serial_port_from_env() -> str:
    """Windows: COM3、COM16…；Linux: /dev/ttyUSB0 等。优先环境变量，便于不改代码换口。"""
    for key in ("SERIAL_PORT", "RUISA_SERIAL_PORT"):
        v = os.getenv(key)
        if v and str(v).strip():
            return str(v).strip()
    return "COM8"


def _serial_baud_from_env() -> int:
    raw = os.getenv("SERIAL_BAUD", "115200")
    try:
        return int(raw)
    except ValueError:
        return 115200


# ================================================================
#  硬件（机械臂下位机串口，协议见 arm.py cmd / MOVE / PING 等）
# ================================================================
SERIAL_PORT = _serial_port_from_env()
SERIAL_BAUD = _serial_baud_from_env()
CAMERA_INDEX = 1
CAMERA_ROT   = True   # 摄像头倒装旋转 180°

# ================================================================
#  初始姿态（所有任务开始/结束后回归此位置）
# ================================================================
HOME = dict(x=15.21, y=-1.28, z=12.52, pitch=-27.4, dur=2000)

# ================================================================
#  唤醒词
# ================================================================
WAKE_MODEL     = "hey_jarvis"
WAKE_THRESHOLD = 0.5
WAKE_COOLDOWN  = 2.0   # 触发后冷却时间（秒）

# ================================================================
#  任务1：颜色分拣
# ================================================================
MAP_FILE = Path(__file__).parent.parent.parent.parent / "model/calibration/teach_map.npz"

OBS_X, OBS_Y, OBS_Z, OBS_PITCH = 16.0, 0.0, -3.2, -76.1   # 观测位姿
SAFE_Z          = 4.0    # 安全运输高度（cm）
X_BIAS, Y_BIAS  = -0.2, 0.3  # 像素映射偏差补偿（cm）
SENSOR_X_OFFSET = 8.7    # 超声波传感器 X 偏移（cm）
ULTRA_MAX_DIFF  = 4.0    # 超声波与相机估算最大允许差值（cm）
ABOVE_CLEARANCE = 1.5    # 物块顶面上方安全间隙（cm）
# 视觉映射 z 略偏低时易蹭桌面：夹取终点在 bz 基础上抬高（cm），与 clamp.py 策略一致
GRASP_Z_LIFT   = 0.8
HORIZ_X, HORIZ_Y, HORIZ_PITCH = 14.68, 0.25, -54.7  # 夹持过渡位姿

ZONES = {
    'r': dict(name="Red",   x=8.31,  y=21.78, z=-6.54, pitch=-74.2),
    'g': dict(name="Green", x=0.84,  y=22.21, z=-7.01, pitch=-78.0),
    'b': dict(name="Blue",  x=-6.36, y=21.34, z=-7.24, pitch=-72.2),
}

# ================================================================
#  任务2：智能台灯
# ================================================================
LED_X, LED_Y, LED_Z, LED_PITCH = 15.21, -1.28, 12.52, -27.4
LED_PRESETS = {
    "off":    dict(r=0,   g=0,   b=0,   bright=0),
    "low":    dict(r=180, g=160, b=120, bright=60),
    "medium": dict(r=255, g=200, b=150, bright=128),
    "high":   dict(r=255, g=230, b=200, bright=220),
}
LED_DEFAULT = "medium"

# ================================================================
#  任务3：人脸追踪
# ================================================================
FACE_Z, FACE_PITCH, FACE_R   = 21.08, -1.0, 16.52
FACE_ANGLE_MIN, FACE_ANGLE_MAX, FACE_ANGLE_CENTER = -50.9, 52.1, -1.7
FACE_SCALE     = 0.030   # °/px
FACE_DIRECTION = -1      # 追踪方向（-1 反向）
FACE_DEADZONE  = 50      # px，小于此不动
FACE_MOVE_DUR  = 400     # ms，每次调整时长
FACE_COOLDOWN  = 0.6     # s，调整冷却
FACE_SCAN_STEP = 4.0     # °，扫描步长
FACE_SCAN_DUR  = 150     # ms，扫描每步时长
FACE_SCAN_CYCLES = 2     # 扫描圈数
FACE_TASK_TIMEOUT = 60   # s，任务最长运行时间

# ================================================================
#  任务3.5（任务5）：动作回放
# ================================================================
ACTIONS_DIR = Path(__file__).parent.parent.parent / "target" / "action" / "actions"

# 回放时序（与 action_teach.py 保持一致）
ACTION_PLAYBACK_DUR_MS      = 250   # 每帧舵机运动时长 ms
ACTION_PLAYBACK_INTERVAL_MS = 170   # 相邻帧发送间隔 ms
ACTION_PAUSE_THRESHOLD_MS   = 350   # 超过此值视为有意停顿，改用阻塞 MOVE

# 语音关键词 → 动作文件名（文件名须与 actions/ 目录中 JSON 文件名一致）
ACTION_KEYWORD_MAP = {
    "跳舞":   ["跳舞", "跳个舞", "dance", "舞蹈"],
    "打招呼": ["打招呼", "招手", "挥手", "你好", "hello", "hi"],
    "点头":   ["点头", "同意", "好的", "赞同"],
    "摇头":   ["摇头", "不", "拒绝", "反对"],
    "看天气": ["天气", "看天气", "抬头看", "温度"],
}

# ================================================================
#  任务4：题目解答
# ================================================================
ANSWER_OBS_X, ANSWER_OBS_Y = 16.0, 0.0  # 拍照位姿（与分拣观测位相同）
ANSWER_OBS_Z, ANSWER_OBS_PITCH = -3.2, -76.1
PHOTO_PATH     = Path(__file__).parent / "_tmp_photo.jpg"
DASHSCOPE_BASE_URL = "https://dashscope.aliyuncs.com/compatible-mode/v1"
VL_MODEL       = "qwen-vl-plus"
LLM_MODEL      = "qwen-turbo"

# ================================================================
#  API Key（优先读环境变量）
# ================================================================
DASHSCOPE_API_KEY = (
    os.getenv("DASHSCOPE_API_KEY")
    or os.getenv("QWEN_API_KEY")
    or "sk-ec2d73d7868b49e89cf080f412ce687e"
)

# ================================================================
#  STT：语音识别录音时长
# ================================================================
STT_PHRASE_LIMIT = 3    # 每次最长录音时长（秒）

# ================================================================
#  TTS：语音合成（DashScope CosyVoice）
# ================================================================
# TTS（qwen3-tts-flash，与 xiaoshutong 保持一致）
TTS_MODEL = "qwen3-tts-flash"
TTS_VOICE = "Cherry"         # 可选: Cherry(女) / Ryan(男) / Ethan(男)

# ASR（qwen3-asr-flash）
ASR_MODEL = "qwen3-asr-flash"

