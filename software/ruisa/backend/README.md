# Ruisa Agent Web 后端

精简版：仅提供 **WebSocket 对话桥接**，逻辑对齐本目录下 `agent/agent.py`，任务执行直接调用 `agent/tasks.py`（五项 `task_*`）。**Agent 源码已并入 `backend/agent/`，与 FastAPI 同仓，便于部署。**

## 运行

```bash
cd software/ruisa/backend
pip install -r requirements.txt
# 可选：在 software/ruisa/.env 中设置 AGENT_DEBUG_KEYBOARD=1
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

浏览器打开 `http://127.0.0.1:8000/`（由 FastAPI 挂载 `../frontend/index.html`）。

若使用 **Spring Boot + Vue3** 栈：先保持本 Python 服务在 `8000`，再启动 `../backend-spring`（默认 `8080`）与 `../front-vue`（`npm run dev`，见各目录说明）；网关将 WS/MJPEG 转发到本进程，任务逻辑不变。

单机调试 Agent（不启 Web）：

```bash
cd software/ruisa/backend/agent
python agent.py
```

## 配置

- 机械臂串口与本地 `python agent.py` 相同：在 `agent/config.py` 中改默认 `SERIAL_PORT`，或在 `.env` 中设置 `SERIAL_PORT=COM3`（Windows）或 `SERIAL_BAUD=115200`。详见 `agent/arm.py`（`pyserial.Serial`）。
- 摄像头：见 `agent/config.py` 中 `CAMERA_INDEX` 等（`tasks.py` 内 `_open_camera` 按序号枚举）。
- `AGENT_DEBUG_KEYBOARD`：为 `1` 时须先在页面「模拟唤醒」再发指令（等同 `DEBUG=1 python agent.py`）。

## API

- `GET /health`
- `WS /api/v1/ws/agent/chat` — Agent 对话与任务推送（无需登录）
- `GET /api/v1/camera/mjpeg` — 机械臂摄像头 MJPEG 流（任务 `clamp` / `face` / `answer` 运行时有画面，由 `agent/camera_preview.py` 推送帧）
