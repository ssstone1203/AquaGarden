# MCU 水泵 UART 控制使用说明

## 概述

本文档说明如何通过前端 Web 界面控制 MCU（RA6E2）板载水泵，实现完整的控制链路：

```
前端 (Vue) → 后端 (Spring Boot) → serial_bridge.py → UART 串口 → MCU → 水泵
```

该方案与原有树莓派 Bridge 控制路径并行存在，适用于 MCU 直连串口的场景。

---

## 架构图

```
┌─────────────┐     POST /api/mcu/pump     ┌──────────────────┐
│   前端 Vue   │ ──────────────────────────→ │  Spring Boot 后端 │
│  RobotView   │                            │ McuPumpController │
└─────────────┘                            └────────┬─────────┘
                                                     │ 内存队列
                                                     │ (McuCommandService)
                                                     ▼
┌─────────────┐   GET /api/mcu/pump/pending ┌──────────────────┐
│     MCU     │ ←──── UART 串口 (115200) ────│ serial_bridge.py │
│   RA6E2     │                             │  (每 0.5s 轮询)   │
└─────────────┘                             └──────────────────┘
```

---

## 组件说明

### 1. 后端：McuCommandService

**文件位置：** `software/fish-arm/backend-spring/src/main/java/com/aquagarden/service/McuCommandService.java`

内存级命令队列服务，使用 `AtomicReference` 实现单条覆盖式存储：

- **覆盖式语义**：新命令自动覆盖旧命令，水泵控制只需执行最新意图
- **线程安全**：支持前端并发写入和 bridge 并发读取

命令类型常量（与 MCU 固件一致）：

| 常量 | 值 | 说明 |
|---|---|---|
| `CMD_STOP` | `0x01` | 停止水泵 |
| `CMD_START` | `0x02` | 启动水泵 |
| `CMD_SET_PWM` | `0x03` | 设置 PWM 力度 |

### 2. 后端：McuPumpController

**文件位置：** `software/fish-arm/backend-spring/src/main/java/com/aquagarden/web/McuPumpController.java`

提供 3 个 REST API 端点：

#### POST /api/mcu/pump

前端调用，入队一条水泵命令。

**请求头：**
```
Authorization: Bearer <JWT_TOKEN>
Content-Type: application/json
```

**请求体：**
```json
{
  "action": "start",   // "stop" | "start" | "set_pwm"
  "power": 80          // 0-100，stop 时可省略
}
```

**成功响应 (200)：**
```json
{
  "ok": true,
  "action": "start",
  "power": 80
}
```

**错误响应 (400)：**
```json
{
  "ok": false,
  "message": "未知 action: xxx，支持 stop / start / set_pwm"
}
```

#### GET /api/mcu/pump/pending

serial_bridge.py 轮询调用，取出并清空待发送命令。

**认证：** 无需 JWT（已加入安全白名单）

**有待发命令时响应 (200)：**
```json
{
  "cmd": 2,
  "power": 80,
  "cmdName": "start"
}
```

**无待发命令时响应：** `204 No Content`

#### GET /api/mcu/pump/status

调试接口，查看当前队列状态。

**认证：** 无需 JWT

**响应 (200)：**
```json
{
  "pending": true,
  "cmd": 2,
  "power": 80,
  "cmdName": "start"
}
```

### 3. serial_bridge.py

**文件位置：** `software/fish-arm/serial_bridge.py`

新增功能：

- `build_mcu_pump_frame(cmd, power)` — 构建 38 字节 UART 下行帧
- `mcu_command_poller()` — 后台线程，每 0.5s 轮询后端待发命令
- 串口写入锁（`threading.Lock`）保证线程安全

**UART 帧格式（38 字节）：**

| 偏移 | 长度 | 值 | 说明 |
|---|---|---|---|
| 0 | 1 | `0x55` | 同步头 SYNC0 |
| 1 | 1 | `0xAA` | 同步头 SYNC1 |
| 2 | 1 | `0x01/0x02/0x03` | 命令类型 |
| 3 | 1 | `0-100` | 水泵力度 |
| 4-5 | 2 | `0x1E 0x00` | payload_len = 30（小端） |
| 6-35 | 30 | `0x00` | 填充（保留） |
| 36-37 | 2 | CRC16 | Modbus CRC-16 校验 |

### 4. 前端：RobotView.vue

**文件位置：** `software/fish-arm/front-vue/src/views/RobotView.vue`

新增 "MCU 水泵控制" 面板，包含：

- PWM 滑块（0-100%）
- 数值输入框
- 三个操作按钮：开泵 / 设置 PWM / 关泵
- 命令路径提示文字

---

## 启动步骤

### 1. 启动后端

```bash
cd software/fish-arm/backend-spring
./mvnw spring-boot:run
# 或
mvn spring-boot:run
```

后端默认端口：`8090`

### 2. 启动 serial_bridge.py

```bash
cd software/fish-arm

# 基本用法
python serial_bridge.py --port COM3 --backend http://localhost:8090

# Linux
python serial_bridge.py --port /dev/ttyUSB0 --backend http://127.0.0.1:8090

# 启用详细日志
python serial_bridge.py --port COM3 --backend http://localhost:8090 --verbose
```

启动后会看到：
```
串口：COM3 @ 115200 波特，后端：http://localhost:8090
串口已打开，开始监听…
MCU 命令轮询线程已启动，间隔 0.5 s
```

### 3. 启动前端

```bash
cd software/fish-arm/front-vue
npm install
npm run dev
```

访问 `http://localhost:5173`，登录后进入"机械臂控制"页面（`/robot`）。

---

## 使用方法

### 前端操作

1. 打开机械臂控制页面（`/robot`）
2. 滚动到 **"MCU 水泵控制"** 面板
3. 使用滑块或输入框设置 PWM 力度（0-100%）
4. 点击按钮：
   - **开泵**：启动水泵，使用当前 PWM 值
   - **设置 PWM**：运行中调整水泵力度
   - **关泵**：停止水泵

### curl 命令行测试

```bash
# 获取 JWT（先登录）
TOKEN=$(curl -s -X POST http://localhost:8090/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}' | jq -r '.token')

# 启动水泵，PWM=80%
curl -X POST http://localhost:8090/api/mcu/pump \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"action":"start","power":80}'

# 设置 PWM=50%
curl -X POST http://localhost:8090/api/mcu/pump \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"action":"set_pwm","power":50}'

# 停止水泵
curl -X POST http://localhost:8090/api/mcu/pump \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"action":"stop"}'

# 查看待发命令（调试，无需认证）
curl http://localhost:8090/api/mcu/pump/status
```

---

## MCU 固件对应关系

### 命令映射

| 前端 action | API cmd 值 | MCU 宏定义 | 固件行为 |
|---|---|---|---|
| `stop` | `0x01` | `UART_CMD_PUMP_STOP` | `g_pump_manual_mode=1, g_pump_manual_power_percent=0` |
| `start` | `0x02` | `UART_CMD_PUMP_START` | `g_pump_manual_mode=1, 设置 power` |
| `set_pwm` | `0x03` | `UART_CMD_PUMP_SET_PWM` | `g_pump_manual_mode=1, 更新 power` |

### MCU 固件相关代码

**文件：** `hardware/demo_wyr/FreeRTOS drive/data_merge/src/Communicate_Task_entry.c`

命令解析函数（第 272-302 行）：

```c
static void uart_handle_pump_command(const uint8_t *frame)
{
    uint8_t cmd = frame[2];    // 帧偏移 [2] = 命令类型
    uint8_t power = frame[3];  // 帧偏移 [3] = 力度

    switch (cmd)
    {
    case UART_CMD_PUMP_STOP:
        g_pump_manual_mode = 1U;
        g_pump_manual_power_percent = 0U;
        break;

    case UART_CMD_PUMP_START:
        g_pump_manual_mode = 1U;
        if (power == 0U) power = 60U;  // 默认 60%
        if (power > 100U) power = 100U;
        g_pump_manual_power_percent = power;
        g_pump_cycle_power_percent = power;
        break;

    case UART_CMD_PUMP_SET_PWM:
        g_pump_manual_mode = 1U;
        if (power > 100U) power = 100U;
        g_pump_manual_power_percent = power;
        g_pump_cycle_power_percent = power;
        break;
    }
}
```

---

## 故障排查

### 命令未生效

1. **检查 serial_bridge.py 是否运行**
   ```bash
   # 查看日志中是否有 "MCU 命令轮询线程已启动"
   ```

2. **检查后端是否收到命令**
   ```bash
   curl http://localhost:8090/api/mcu/pump/status
   # 应返回 {"pending": false}
   ```

3. **检查串口连接**
   ```bash
   # 确认 COM 端口号正确
   # 确认波特率 115200
   # 确认 MCU 固件已启用 UART 模式（USE_UART_COMM 宏已定义）
   ```

4. **查看 serial_bridge.py 日志**
   ```
   MCU 水泵命令已发送：start power=80 (38 bytes)
   ```
   如果看到 `MCU 命令写入失败`，说明串口断开或被占用。

### 前端报错 "MCU 水泵控制失败"

1. 检查 JWT Token 是否过期（重新登录）
2. 检查后端是否运行在 8090 端口
3. 检查浏览器控制台网络请求详情

### 命令延迟

- 轮询间隔 0.5 秒，最大延迟约 0.5 秒
- 如需更低延迟，修改 `serial_bridge.py` 中 `MCU_CMD_POLL_SEC` 常量

---

## 安全说明

| 端点 | 认证要求 | 说明 |
|---|---|---|
| `POST /api/mcu/pump` | 需要 JWT | 前端调用，受保护 |
| `GET /api/mcu/pump/pending` | 无需 JWT | bridge 内部轮询，已加入白名单 |
| `GET /api/mcu/pump/status` | 无需 JWT | 调试接口，已加入白名单 |

白名单配置：`SecurityWhitelist.java`

---

## 文件清单

```
新增文件：
  backend-spring/.../service/McuCommandService.java    # 命令队列服务
  backend-spring/.../web/McuPumpController.java        # REST API 控制器
  docs/MCU水泵UART控制使用说明.md                       # 本文档

修改文件：
  backend-spring/.../security/SecurityWhitelist.java   # 添加白名单端点
  software/fish-arm/serial_bridge.py                   # 添加命令轮询线程
  front-vue/src/views/RobotView.vue                    # 添加 MCU 泵控制面板
```
