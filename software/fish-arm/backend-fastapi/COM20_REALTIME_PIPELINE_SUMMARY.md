# COM20 传感器实时数据链路实施总结

## 1. 项目目标

本次工作的目标是在当前 Windows 计算机上实现并验证以下实时数据链路：

```text
MCU 传感器
    -> USB-SERIAL CH340（COM20，115200 baud）
    -> FastAPI 后端
    -> WebSocket 实时推送
    -> front-vue 仪表板实时显示
```

实施日期：2026-07-15。

协议权威来源：`hardware/ra8p1/aquagarden/通信协议.md`、`命令表.md` 和 `module/com_protocol.c`。后续不再使用其他旧固件目录推断字段。

## 2. 实施前发现的问题

| 问题 | 影响 |
|---|---|
| FastAPI 默认使用 COM4，而当前 MCU 实际连接到 COM20 | 后端无法直接打开正确串口 |
| 串口线程使用 `asyncio.run()` 广播 WebSocket 消息 | 跨线程、跨事件循环操作 WebSocket，实时推送可能失效 |
| 后端发送 `type: sensor`，front-vue 只处理 `type: sensor_data` | 前端虽然建立 WebSocket 连接，但会忽略传感器消息 |
| WebSocket 连接建立后不主动发送当前快照 | 页面需要等待下一帧硬件数据才能显示 |
| SQLite 写入异常会冒泡到串口读循环 | 历史数据库不可写时，串口连接也会被关闭 |
| 文档声明支持 `AQUAGARDEN_DATABASE_URL`，代码未绑定该环境变量 | 无法按文档切换数据库 |

## 3. 完成的主要更改

### 3.1 COM20 串口配置

- 默认串口从 `COM4` 调整为 `COM20`。
- 默认波特率保持 `115200`。
- 保留环境变量覆盖能力：
  - `AQUAGARDEN_HARDWARE_SERIAL_ENABLED`
  - `AQUAGARDEN_HARDWARE_SERIAL_PORT`
  - `AQUAGARDEN_HARDWARE_SERIAL_BAUD`
- 串口断开后仍按配置的重连间隔自动重连。

相关文件：[`app/core/config.py`](app/core/config.py)、[`.env.example`](.env.example)。

### 3.2 线程安全的 WebSocket 广播

- FastAPI 启动时获取并绑定主事件循环。
- 串口后台线程不再直接执行异步 WebSocket 方法。
- 串口线程通过 `loop.call_soon_threadsafe()` 将广播任务提交到 FastAPI 主事件循环。
- FastAPI 关闭时停止串口线程并解除事件循环绑定。
- 应用生命周期从已弃用的 `on_event` 调整为 FastAPI lifespan。

相关文件：[`app/main.py`](app/main.py)、[`app/services/logs.py`](app/services/logs.py)、[`app/services/hardware_serial.py`](app/services/hardware_serial.py)。

### 3.3 统一前后端实时消息契约

后端现在统一发送 front-vue 能直接处理的 `sensor_data` 消息。字段同时保持扁平形式和嵌套 `data` 形式，方便不同客户端使用：

```json
{
  "type": "sensor_data",
  "source": "hardware",
  "ts": 1784123426006,
  "water_temp": 17.6,
  "air_temp": 18.7,
  "air_humidity": 77.9,
  "wqi": 0.0,
  "soil_moisture": 100.0,
  "tds_ntu": 0,
  "pump_pwm": 60,
  "need_watering": true,
  "atomizer_state": 0,
  "usb_light_mode": 255,
  "alarm_flags": 68,
  "alarms": ["air_read_fail", "tds_low"],
  "data": {
    "water_temp": 17.6,
    "air_temp": 18.7,
    "air_humidity": 77.9,
    "wqi": 0.0,
    "soil_moisture": 100.0,
    "tds_ntu": 0,
    "pump_pwm": 60,
    "need_watering": true,
    "alarm_flags": 68
  }
}
```

其中 `wqi` 只为兼容旧前端和数据库保留，硬件真实字段是 `tds_ntu`。上行严格接收 `VER=0x02`；下行严格使用 `5A A5 LEN CMD PAYLOAD CRC16`，不再发送旧 38 字节 `55 AA` 控制帧。

现有 front-vue 已经订阅 `/ws/logs` 并处理 `sensor_data`，因此不需要修改前端源码即可实时显示。

### 3.4 WebSocket 端点

当前支持两个 WebSocket 地址：

| 地址 | 用途 |
|---|---|
| `ws://127.0.0.1:8090/ws/sensors` | 新增的专用传感器实时端点 |
| `ws://127.0.0.1:8090/ws/logs` | 兼容现有 front-vue |

连接建立后，后端会立即发送当前有效快照；随后每收到一帧有效 MCU 数据，就继续推送新快照。

相关文件：[`app/api/websocket.py`](app/api/websocket.py)、[`app/api/sensors.py`](app/api/sensors.py)。

### 3.5 数据库故障隔离

- 原 `aquagarden.db` 在当前运行环境中能够读取，但数据页写入被操作系统拒绝。
- 历史数据写入失败现在会被捕获、回滚并记录服务器日志。
- 数据库故障只设置通用的 `lastPersistError`，不会再关闭 COM20 或中断 WebSocket 实时推送。
- 已修正 `AQUAGARDEN_DATABASE_URL` 环境变量绑定。
- 使用 SQLite 备份机制，将原数据库内容复制到可写的 `aquagarden-runtime.db`。
- 当前机器通过被 Git 忽略的 `.env` 使用该运行库副本。

本地运行配置如下：

```dotenv
AQUAGARDEN_DATABASE_URL=sqlite:///./aquagarden-runtime.db
AQUAGARDEN_HARDWARE_SERIAL_ENABLED=true
AQUAGARDEN_HARDWARE_SERIAL_PORT=COM20
AQUAGARDEN_HARDWARE_SERIAL_BAUD=115200
```

`.env` 和 `aquagarden-runtime.db` 是当前机器的运行文件，不应提交到版本库。

## 4. 接口说明

| 接口 | 说明 |
|---|---|
| `GET /api/mcu/serial/status` | 查看串口连接、帧计数、CRC、无效帧及持久化错误 |
| `GET /api/sensors` | 获取当前传感器快照及数据来源 |
| `GET /api/sensors/history` | 获取已持久化的历史数据 |
| `POST /api/sensors/ingest` | 通过 HTTP 写入完整传感器快照并广播 |
| `POST /api/sensor/upload` | 通过设备令牌上传单项传感器值并广播 |
| `WS /ws/sensors` | 专用实时传感器推送 |
| `WS /ws/logs` | front-vue 兼容实时推送 |

串口状态中的关键字段：

- `connected`：COM20 当前是否打开。
- `rxFrameCount`：已接收并解析的有效帧数。
- `rxCrcErrorCount`：CRC 校验失败数量。
- `rxInvalidFrameCount`：长度、版本或数值范围不合法的帧数。
- `lastError`：串口连接或读写错误。
- `lastPersistError`：历史数据库持久化错误，不影响实时链路。

## 5. 自动化测试

新增 [`tests/test_sensor_realtime.py`](tests/test_sensor_realtime.py)，覆盖以下行为：

1. `sensor_data` 消息符合 front-vue 消费契约并携带完整 v2 telemetry。
2. WebSocket 新连接立即收到当前传感器快照。
3. 串口后台线程能够把广播任务安全提交到 FastAPI 主事件循环。
4. 数据库持久化失败时，实时状态更新和 WebSocket 广播继续工作。
5. 用户提供的真实 38 字节帧严格按 v2 布局解码。
6. 旧 `VER=0x01` 布局被拒绝。
7. START/STOP/PWM/AUTO/MANUAL 下行帧与固件命令表 HEX 向量一致。

验证命令：

```powershell
python -m pytest -q
$env:PYTHONPYCACHEPREFIX = Join-Path $env:TEMP 'aquagarden-pycache'
python -m compileall -q app main.py
```

最终结果：

```text
11 passed
compileall passed
git diff --check passed
```

## 6. 实际硬件与网络验收

本次已连接真实 COM20 设备完成验收：

| 验收项 | 结果 |
|---|---|
| Windows 串口设备 | `USB-SERIAL CH340 (COM20)` |
| VID:PID | `1A86:7523` |
| 波特率 | `115200` |
| 串口持续连接 | 通过 |
| 2 秒有效帧增长 | 从 1 增至 9 |
| CRC 错误 | 0 |
| 无效帧 | 0 |
| WebSocket 连续消息 | 成功收到初始快照和下一帧硬件更新 |
| 历史记录持久化 | 行数从 221 增至 222 |
| front-vue 连接 | 已成功连接 `/ws/logs` |

验收期间读取到的传感器样本：

```text
水温：17.5 ~ 17.6 °C
空气温度：18.7 °C
空气湿度：77.9 %RH
水质指数：0.0
土壤湿度：100.0 %
```

生成本文档时，串口状态仍为 `connected=true`，有效帧计数为 `634`，CRC 和无效帧计数均为 `0`。

## 7. 启动和检查方式

当前项目入口为根目录的 `main.py`：

```powershell
python main.py
```

启动后可访问：

- FastAPI 文档：<http://127.0.0.1:8090/docs>
- 串口状态：<http://127.0.0.1:8090/api/mcu/serial/status>
- 当前传感器数据：<http://127.0.0.1:8090/api/sensors>

快速检查：

```powershell
curl http://127.0.0.1:8090/api/mcu/serial/status
curl http://127.0.0.1:8090/api/sensors
```

正常情况下应满足：

- `connected` 为 `true`。
- `rxFrameCount` 持续增加。
- `lastError` 和 `lastPersistError` 为 `null`。
- `/api/sensors` 的 `source` 为 `hardware`。
- `ageMs` 保持在较小范围内。

## 8. 涉及的代码和文档文件

| 文件 | 更改说明 |
|---|---|
| `app/core/config.py` | COM20 默认配置、数据库 URL 环境变量绑定 |
| `app/main.py` | FastAPI lifespan、主事件循环绑定、串口服务启停 |
| `app/services/logs.py` | 线程安全广播和统一传感器消息构造 |
| `app/services/hardware_serial.py` | 串口线程广播、停止等待、持久化故障隔离 |
| `app/api/websocket.py` | `/ws/sensors`、连接后立即发送当前快照 |
| `app/api/sensors.py` | HTTP 传感器入口统一使用实时消息契约 |
| `tests/test_sensor_realtime.py` | 实时链路回归测试 |
| `.env.example` | COM20、数据库和安全配置示例 |
| `README.md` | COM20 接入、WebSocket 契约和运行说明 |

## 9. 当前运行状态

FastAPI 已以后台进程运行，监听：

```text
http://127.0.0.1:8090
```

当前 front-vue 可继续通过 `/ws/logs` 接收实时传感器数据；新客户端建议使用语义更明确的 `/ws/sensors`。

最新协议复核后，后台服务进程为 PID `47296`，严格使用 `VER=0x02` 上行和 `5A A5` 下行；真实 COM20 连续收帧、HTTP 和 WebSocket 完整 telemetry 均已通过验证。

当前最终服务进程为 PID `42156`。

## 10. FastAPI 用户与登录补充

- 保留 `POST /api/register`、`POST /api/login` 和 HS256 JWT Bearer 契约。
- 新增环境变量驱动的初始管理员创建，密码使用 bcrypt，且重复启动不会重置已有用户。
- 新增 `GET /api/users/me` 返回当前登录用户。
- `GET /api/users` 从公开接口调整为仅管理员访问。
- 初始管理员凭据仅存放在被 Git 忽略的 `.env`，`.env.example` 不包含真实密码。
- JWT 失败统一返回前端可识别的通用错误，过期或无效 token 可被 front-vue 自动清除。
- 新增 7 项认证测试；当前全量测试为 `18 passed`。
