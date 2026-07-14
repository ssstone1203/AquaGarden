# AquaGarden FastAPI Backend

本目录是 AquaGarden 的 FastAPI 后端实现，与同级 `backend-spring` 目录中的 Spring Boot 后端分开维护。默认端口仍为 `8090`，前端 Vue3 的 `/api/...` 与 `/ws/logs` 调用无需改路径。

如需复用 Spring 后端现有 SQLite 数据库，可将 `backend-spring/aquagarden.db` 复制到本目录，或通过 `AQUAGARDEN_DATABASE_URL` 指向该数据库。

## 运行

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python run_fastapi.py
```

或：

```powershell
uvicorn app.main:app --host 0.0.0.0 --port 8090 --reload
```

## COM4 USB 传感器直连

FastAPI 后端默认启用本机串口直连，端口为 `COM4`、波特率 `115200`。RA8P1 使用 `hardware/ra8p1_wyr/merge` 工程中的 `Actuator_Comm_Task_entry.c`，每 250ms 发送一帧：

- 帧头：`55 AA`
- 版本：`01` 或 `02`
- Payload：30 字节
- 总长度：38 字节
- 校验：CRC16/Modbus，小端

启动后端后可用以下接口确认链路：

```powershell
curl http://127.0.0.1:8090/api/mcu/serial/status
curl http://127.0.0.1:8090/api/sensors
```

如果电脑枚举出的 USB 串口不是 COM4，可覆盖环境变量：

```powershell
$env:AQUAGARDEN_HARDWARE_SERIAL_ENABLED="true"
$env:AQUAGARDEN_HARDWARE_SERIAL_PORT="COM4"
$env:AQUAGARDEN_HARDWARE_SERIAL_BAUD="115200"
python run_fastapi.py
```

## 已迁移接口

- 认证：`POST /api/register`、`POST /api/login`
- 用户：`GET /api/users`
- 传感器：`GET /api/sensors`、`GET /api/sensor/latest`、`GET /api/sensors/history`、`POST /api/sensors/ingest`、`POST /api/sensor/upload`
- 机器人与模式：`/api/robot/*`、`/api/mode`
- Aqua 控制：`/api/aqua/*`、`/api/control/pump`
- MCU 水泵队列：`/api/mcu/pump*`
- 视频：`/api/video/robot`、`/api/video/tank`、`/api/video/tank/snapshot`、`/api/video/tank/status`、`/api/video/tank/detections`、`/api/video/tank/ingest`
- AI：`/api/ai/ecosystem-analysis`、`/api/ai/chat`
- WebSocket：`/ws/logs`

## 兼容说明

- JWT 使用 `Authorization: Bearer <token>`，算法固定为 `HS256`。
- 密码使用 bcrypt 哈希。
- CORS 默认与原 Spring 配置一致，允许任意来源但不允许凭据。
- Spring 中直接依赖 Java 串口库的硬件直连逻辑，在 FastAPI 版本中已由 `app/services/hardware_serial.py` 接管。默认使用 COM4，也可通过 `AQUAGARDEN_HARDWARE_SERIAL_PORT`、`AQUAGARDEN_HARDWARE_SERIAL_BAUD` 覆盖。
- 若仍保留本机 `serial_bridge.py`，可启用 `AQUAGARDEN_SERIAL_PUMP_ENABLED=true` 作为水泵后备。
- 现有 SQLite 里的 `users.created_at` 和 `sensor_readings.recorded_at` 兼容整数毫秒时间戳，不需要先清库。
- LLM 未配置 API Key 或调用失败时，会返回规则引擎回退结果，保持前端功能可用。

## 环境变量

常用配置：

```powershell
$env:AQUAGARDEN_FASTAPI_PORT="8090"
$env:AQUAGARDEN_JWT_SECRET="change-me-to-a-long-random-secret"
$env:AQUAGARDEN_DEVICE_UPLOAD_TOKEN="123456789"
$env:ANTHROPIC_API_KEY="<optional>"
```
