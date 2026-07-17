# AquaGarden FastAPI Backend

本目录是 AquaGarden 的 FastAPI 后端实现，与同级 `backend-spring` 目录中的 Spring Boot 后端分开维护。默认端口仍为 `8090`，前端 Vue3 的 `/api/...` 与 `/ws/logs` 调用无需改路径。

如需复用 Spring 后端现有 SQLite 数据库，可将 `backend-spring/aquagarden.db` 复制到本目录，或通过 `AQUAGARDEN_DATABASE_URL` 指向该数据库。

如果已有数据库文件因 Windows 权限无法写入，可先复制为可写副本，再通过 `AQUAGARDEN_DATABASE_URL=sqlite:///./aquagarden-runtime.db` 使用副本。实时串口推送不会因历史数据库写入失败而中断，具体状态可查看 `lastPersistError`。

## 运行

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python main.py
```

或：

```powershell
uvicorn app.main:app --host 0.0.0.0 --port 8090 --reload
```

## COM20 USB 传感器直连

FastAPI 后端默认启用本机串口直连，端口为 `COM20`、波特率 `115200`。通信实现以 `hardware/ra8p1/aquagarden/通信协议.md` 和 `module/com_protocol.c` 为准，MCU 每 250ms 发送一帧：

- 帧头：`55 AA`
- 版本：`02`（应用协议 v1 的上行版本字节）
- Payload：30 字节
- 总长度：38 字节
- 校验：CRC16/Modbus，小端
- 主要字段：气温、空气湿度、水温、土壤湿度、TDS/浊度 NTU、水泵 PWM、联动状态、雾化器、USB 灯、告警和重试计数

Host 到 MCU 的控制帧使用 `5A A5 LEN CMD PAYLOAD CRC16`。水泵推荐命令为启动 `0x04`、停止 `0x05`、PWM `0x06`、自动 `0x07`；固件不再支持旧的 38 字节 `55 AA` 下行控制帧。

启动后端后可用以下接口确认链路：

```powershell
curl http://127.0.0.1:8090/api/mcu/serial/status
curl http://127.0.0.1:8090/api/sensors
```

如果电脑枚举出的 USB 串口不是 COM20，可覆盖环境变量：

```powershell
$env:AQUAGARDEN_HARDWARE_SERIAL_ENABLED="true"
$env:AQUAGARDEN_HARDWARE_SERIAL_PORT="COM20"
$env:AQUAGARDEN_HARDWARE_SERIAL_BAUD="115200"
python main.py
```

也可以复制 `.env.example` 为 `.env` 后直接修改配置。后端会在串口断开时自动重连。

### 实时推送

前端可连接 `ws://127.0.0.1:8090/ws/sensors`；现有 front-vue 使用的 `/ws/logs` 也保持兼容。连接建立后会立即返回当前快照，此后每收到一帧有效串口数据就推送一次：

```json
{
  "type": "sensor_data",
  "source": "hardware",
  "ts": 1784083200000,
  "water_temp": 24.7,
  "air_temp": 26.1,
  "air_humidity": 58.2,
  "wqi": 0.0,
  "soil_moisture": 63.0,
  "tds_ntu": 0,
  "pump_pwm": 60,
  "need_watering": true,
  "alarm_flags": 68,
  "alarms": ["air_read_fail", "tds_low"],
  "data": {
    "water_temp": 24.7,
    "air_temp": 26.1,
    "air_humidity": 58.2,
    "wqi": 0.0,
    "soil_moisture": 63.0,
    "tds_ntu": 0,
    "pump_pwm": 60,
    "need_watering": true,
    "alarm_flags": 68,
    "alarms": ["air_read_fail", "tds_low"]
  }
}
```

`wqi` 暂时保留为旧前端/数据库兼容字段；实际硬件水质字段以 `tds_ntu` 为准。

## 仪表板双视频流

- 左侧树莓派视频：FastAPI 固定代理 `AQUAGARDEN_RASPBERRY_PI_CAMERA_URL`，默认
  `http://10.213.133.50:18080/video/rgb.mjpg`，前端访问 `/api/video/raspberry-pi`。
- 右侧鱼缸视频：FastAPI 启动时直接打开电脑 USB 摄像头，前端访问 `/api/video/tank`。
  后端使用 `model/yolo_fish/runs/yolo11n_fish_new/weights/best.pt` 对 USB 画面进行金鱼实时识别，
  并把检测框和置信度叠加到 MJPEG 视频中。采集与检测状态位于 `/api/video/tank/status`。
- 可通过 `AQUAGARDEN_TANK_USB_CAMERA_INDEX`、`AQUAGARDEN_TANK_USB_CAMERA_WIDTH`、
  `AQUAGARDEN_TANK_USB_CAMERA_HEIGHT`、`AQUAGARDEN_TANK_USB_CAMERA_FPS` 调整 USB 采集参数。
- 可通过 `AQUAGARDEN_TANK_YOLO_CONFIDENCE`、`AQUAGARDEN_TANK_YOLO_IMAGE_SIZE`、
  `AQUAGARDEN_TANK_YOLO_EVERY_N_FRAMES` 调整识别阈值、输入尺寸和推理间隔。
- 如果 USB 摄像头已被微信、浏览器或其他采集程序占用，先关闭占用程序再启动 FastAPI。

## 已迁移接口

- 认证：`POST /api/register`、`POST /api/login`
- 用户：`GET /api/users/me`（已登录用户）、`GET /api/users`（仅管理员）
- 传感器：`GET /api/sensors`、`GET /api/sensor/latest`、`GET /api/sensors/history`、`POST /api/sensors/ingest`、`POST /api/sensor/upload`
- 机器人与模式：`/api/robot/*`、`/api/mode`
- Aqua 控制：`/api/aqua/*`、`POST /api/aqua/atomizer`、`/api/control/pump`
- MCU 水泵队列：`/api/mcu/pump*`
- 视频：`/api/video/robot`、`/api/video/tank`、`/api/video/tank/snapshot`、`/api/video/tank/status`、`/api/video/tank/detections`、`/api/video/tank/ingest`
- AI：`/api/ai/ecosystem-analysis`、`/api/ai/chat`
- WebSocket：`/ws/sensors`、`/ws/logs`（兼容现有 front-vue）

## 兼容说明

- JWT 使用 `Authorization: Bearer <token>`，算法固定为 `HS256`。
- 密码使用 bcrypt 哈希。
- CORS 默认与原 Spring 配置一致，允许任意来源但不允许凭据。
- FastAPI 的硬件直连由 `app/services/hardware_serial.py` 接管，并严格按 `hardware/ra8p1/aquagarden` 的冻结协议收发。默认使用 COM20，也可通过 `AQUAGARDEN_HARDWARE_SERIAL_PORT`、`AQUAGARDEN_HARDWARE_SERIAL_BAUD` 覆盖。
- 若仍保留本机 `serial_bridge.py`，可启用 `AQUAGARDEN_SERIAL_PUMP_ENABLED=true` 作为水泵后备。
- 现有 SQLite 里的 `users.created_at` 和 `sensor_readings.recorded_at` 兼容整数毫秒时间戳，不需要先清库。
- LLM 未配置 API Key 或调用失败时，会返回规则引擎回退结果，保持前端功能可用。

## 传感器上下文 AI 问答

- `POST /api/ai/chat` 接收用户问题和最多 12 条最近对话，必须携带 JWT。
- FastAPI 从服务端状态读取新鲜的 MCU 传感器快照，并按水温、空气温湿度、TDS 浊度（NTU）和土壤湿度构造模型上下文；浏览器不提交传感器真值。
- API Key 只从 FastAPI 的 `.env` 或进程环境读取，不会出现在前端请求、响应或错误信息中。
- 响应中的 `sensorSource` 为 `hardware` 或 `demo`，前端据此显示“实时传感器”或“演示基线”。
- 模型未配置、返回为空或调用失败时，接口仍返回基于同一传感器快照的本地规则回复。

## 环境变量

常用配置：

```powershell
$env:AQUAGARDEN_FASTAPI_PORT="8090"
$env:AQUAGARDEN_JWT_SECRET="change-me-to-a-long-random-secret"
$env:AQUAGARDEN_DEVICE_UPLOAD_TOKEN="123456789"
$env:AQUAGARDEN_LLM_PROVIDER="anthropic"
$env:AQUAGARDEN_LLM_BASE_URL="https://api.kimi.com/coding"
$env:AQUAGARDEN_LLM_AUTH_MODE="bearer"
$env:AQUAGARDEN_LLM_API_KEY="<backend-only-secret>"
$env:AQUAGARDEN_LLM_MODEL="kimi-for-coding"
```

`AQUAGARDEN_LLM_API_KEY` 也兼容 `ANTHROPIC_AUTH_TOKEN`、`ANTHROPIC_API_KEY` 和 `OPENAI_API_KEY`。Kimi Coding 使用默认的 `bearer`；直连 Anthropic Messages API 时将 `AQUAGARDEN_LLM_AUTH_MODE` 设为 `x-api-key`。

### 初始管理员

FastAPI 可在启动时按环境变量创建一次初始管理员：

```powershell
$env:AQUAGARDEN_ADMIN_USERNAME="admin"
$env:AQUAGARDEN_ADMIN_PASSWORD="use-a-strong-password"
$env:AQUAGARDEN_ADMIN_EMAIL="admin@example.com"
python main.py
```

- 用户名和密码必须同时配置；两者都留空则关闭自动创建。
- 密码只从环境或被 Git 忽略的 `.env` 读取，并使用 bcrypt 入库。
- 如果同名用户已经存在，启动过程不会修改其密码或角色。
- 生产环境不要使用前端展示的演示密码。
