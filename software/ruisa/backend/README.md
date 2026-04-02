# Ruisa Agent Web 后端

精简版：仅提供 **WebSocket 对话桥接**，逻辑对齐 `../agent/agent.py`，任务执行直接调用 `../agent/tasks.py`（五项 `task_*`）。

## 运行

```bash
cd software/ruisa/backend
pip install -r requirements.txt
# 可选：在 software/ruisa/.env 中设置 AGENT_DEBUG_KEYBOARD=1
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

浏览器打开 `http://127.0.0.1:8000/`（由 FastAPI 挂载 `../frontend/index.html`）。

## 配置

- 机械臂串口与本地 `python agent.py` 相同：在 `../agent/config.py` 中改默认 `SERIAL_PORT`，或在 `.env` 中设置 `SERIAL_PORT=COM3`（Windows）或 `SERIAL_BAUD=115200`。详见 `../agent/arm.py`（`pyserial.Serial`）。
- 机械臂 USB 摄像头（避免用笔记本内置）：环境变量 `ARM_CAMERA_INDEX`（默认 1，内置常为 0）、或 `ARM_CAMERA_DEVICE` / `CAMERA_DEVICE_PATH`（设备路径）；Windows 可选 `CAMERA_BACKEND=dshow`。逻辑见 `../agent/camera_util.py`。
- 前端「中断动作」→ WebSocket `interrupt` → `task_control.request_cancel()`，任务在检测点协作退出。
- `AGENT_DEBUG_KEYBOARD`：为 `1` 时须先在页面「模拟唤醒」再发指令（等同 `DEBUG=1 python agent.py`）。

## API

- `GET /health`
- `WS /api/v1/ws/agent/chat` — Agent 对话与任务推送（无需登录）
