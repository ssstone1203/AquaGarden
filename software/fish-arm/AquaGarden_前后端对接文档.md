# AquaGarden 前后端对接文档

本文档用于 PC 后端、Web 前端与树莓派 ROS2 控制端对接。

## 1. 系统结构

```text
Web 前端(Vite/React, 5173)
        |
        | HTTP/WebSocket
        v
PC 后端(Spring Boot, 192.168.163.1:8090)
        |
        | HTTP 请求
        v
树莓派 AquaGarden Bridge(树莓派IP:18080)
        |
        | ROS2 topic/service
        v
机械臂 + 滑轨 + RGB/Depth 相机
```

建议：前端只访问 PC 后端，由 PC 后端统一代理树莓派 `18080` 接口，并通过 WebSocket 把任务状态推送给前端。

## 2. 启动和关闭

进入容器：

```bash
docker exec -it -u root ArmPiUltra bash
```

启动 AquaGarden：

```bash
bash /home/ubuntu/AquaGarden/_start.sh
```

关闭 AquaGarden：

```bash
bash /home/ubuntu/AquaGarden/_stop.sh
```

查看日志：

```bash
tail -f /tmp/aqua_sdk.log
tail -f /tmp/aqua_cam.log
tail -f /tmp/aqua_bridge.log
```

## 3. Bridge 基础地址

树莓派端 Bridge 默认监听：

```text
http://树莓派IP:18080
```

如果在树莓派容器内调试，也可以使用：

```text
http://127.0.0.1:18080
```

## 4. HTTP 接口

### 4.1 查询系统状态

```http
GET /api/status
```

示例：

```bash
curl http://树莓派IP:18080/api/status
```

返回示例：

```json
{
  "ok": true,
  "connected": true,
  "busy": false,
  "currentTask": "idle",
  "phase": "idle",
  "railPosition": null,
  "lastError": "",
  "taskSeq": 0,
  "uptimeSec": 71,
  "servoPulse": {
    "1": 220,
    "2": 489,
    "3": 130,
    "4": 842,
    "5": 836,
    "6": 509
  },
  "camera": {
    "hasRgb": true,
    "hasDepth": true,
    "seq": 783,
    "ageSec": 0.036,
    "size": [640, 400]
  }
}
```

重要字段说明：

| 字段 | 含义 |
| --- | --- |
| `ok` | Bridge 是否正常返回 |
| `connected` | ROS2 Bridge 是否在线 |
| `busy` | 是否正在执行任务 |
| `currentTask` | 当前任务，可能是 `idle`、`feed`、`loosen`、`prune` |
| `phase` | 当前阶段，例如 `idle`、`checking_dependencies`、`rail_home`、`rail_move`、`feed`、`loosen`、`prune`、`stopping`、`failed`、`error` |
| `railPosition` | 当前滑轨位置，`0`、`4000` 或 `null` |
| `lastError` | 最近一次错误信息 |
| `taskSeq` | 任务序号，每启动一个任务递增 |
| `servoPulse` | 1-6 号舵机当前记录脉宽 |
| `camera.hasRgb` | 是否已有 RGB 图像 |
| `camera.hasDepth` | 是否已有深度图像 |
| `camera.ageSec` | 最近一帧距现在多少秒 |

### 4.2 查询相机状态

```http
GET /api/camera/status
```

示例：

```bash
curl http://树莓派IP:18080/api/camera/status
```

返回示例：

```json
{
  "hasRgb": true,
  "hasDepth": true,
  "seq": 783,
  "ageSec": 0.036,
  "size": [640, 400]
}
```

### 4.3 执行投喂鱼食

```http
POST /api/task/feed
```

行为：

- 自动控制滑轨到 `4000`
- 等滑轨停止并稳定后再执行机械臂动作
- 执行完成后机械臂回到初始姿态

示例：

```bash
curl -X POST http://树莓派IP:18080/api/task/feed
```

成功返回：

```json
{
  "ok": true,
  "accepted": true,
  "task": "feed",
  "taskSeq": 1
}
```

HTTP 状态码：`202 Accepted`

### 4.4 执行松土

```http
POST /api/task/loosen
```

行为：

- 自动控制滑轨到 `0`
- 等滑轨停止并稳定后再执行机械臂动作
- 执行完成后机械臂回到初始姿态

示例：

```bash
curl -X POST http://树莓派IP:18080/api/task/loosen
```

### 4.5 执行裁剪黄叶

```http
POST /api/task/prune
```

行为：

- 自动控制滑轨到 `0`
- 使用 RGB/Depth 相机检测黄叶
- 找到目标后执行裁剪动作
- 执行完成后机械臂回到初始姿态

示例：

```bash
curl -X POST http://树莓派IP:18080/api/task/prune
```

### 4.6 停止当前任务

```http
POST /api/task/stop
```

示例：

```bash
curl -X POST http://树莓派IP:18080/api/task/stop
```

返回：

```json
{
  "ok": true,
  "message": "stop requested"
}
```

## 5. 错误返回

### 5.1 任务正在执行

如果当前已有任务运行，再请求新任务会返回：

```json
{
  "ok": false,
  "busy": true,
  "message": "task already running"
}
```

HTTP 状态码：`409 Conflict`

前端处理建议：

- 禁用三个任务按钮
- 显示“任务执行中”
- 轮询 `/api/status` 或等待后端 WebSocket 推送

### 5.2 未知任务

请求不存在的任务，例如 `/api/task/xxx`：

```json
{
  "ok": false,
  "message": "unknown task: xxx"
}
```

HTTP 状态码：`400 Bad Request`

### 5.3 任务失败

任务失败后，`/api/status` 中通常会出现：

```json
{
  "busy": false,
  "currentTask": "idle",
  "phase": "failed",
  "lastError": "具体错误信息"
}
```

前端应该显示 `lastError`，并允许用户再次发起任务或点击停止。

## 6. 视频流

### 6.1 RGB 实时画面

```text
http://树莓派IP:18080/video/rgb.mjpg
```

前端可直接使用：

```html
<img src="http://树莓派IP:18080/video/rgb.mjpg" />
```

### 6.2 深度图实时画面

```text
http://树莓派IP:18080/video/depth.mjpg
```

前端可直接使用：

```html
<img src="http://树莓派IP:18080/video/depth.mjpg" />
```

说明：深度图已经在 Bridge 内转换为伪彩色 MJPEG，前端不需要自己解析原始深度数据。

## 7. 后端开发建议

建议 PC 后端提供自己的业务 API，例如：

```http
GET  /api/aqua/status
POST /api/aqua/tasks/feed
POST /api/aqua/tasks/loosen
POST /api/aqua/tasks/prune
POST /api/aqua/tasks/stop
GET  /api/aqua/video/rgb
GET  /api/aqua/video/depth
```

后端内部再转发到树莓派：

```text
http://树莓派IP:18080/api/status
http://树莓派IP:18080/api/task/feed
http://树莓派IP:18080/api/task/loosen
http://树莓派IP:18080/api/task/prune
http://树莓派IP:18080/api/task/stop
```

后端需要做的事情：

- 统一配置树莓派 Bridge 地址，例如 `aqua.bridge.base-url=http://树莓派IP:18080`
- 对任务请求做互斥保护，避免前端重复点击
- 把 Bridge 返回的 `202`、`409`、`400` 原样或规范化返回给前端
- 定时轮询 `/api/status`，通过 WebSocket 推送给前端
- 记录任务开始时间、结束时间、任务结果和错误信息
- 视频流可以直接让前端访问树莓派，也可以由后端做反向代理

## 8. 前端开发建议

页面建议包含：

- 系统状态卡片：在线、忙碌、当前任务、当前阶段、滑轨位置、错误信息
- 三个任务按钮：投喂鱼食、松土、裁剪黄叶
- 一个停止按钮：停止当前任务
- RGB 画面窗口
- 深度图画面窗口
- 日志面板：显示后端 WebSocket 推送的任务状态变化

按钮状态建议：

- `busy=true` 时禁用 `feed`、`loosen`、`prune`
- `busy=true` 时启用 `stop`
- `phase=failed` 或 `phase=error` 时突出显示 `lastError`
- `camera.hasRgb=false` 或 `camera.hasDepth=false` 时提示相机未就绪

前端轮询示例：

```ts
async function fetchAquaStatus() {
  const res = await fetch('/api/aqua/status')
  if (!res.ok) throw new Error(`status request failed: ${res.status}`)
  return await res.json()
}

setInterval(async () => {
  const status = await fetchAquaStatus()
  console.log(status)
}, 1000)
```

执行任务示例：

```ts
async function startTask(task: 'feed' | 'loosen' | 'prune') {
  const res = await fetch(`/api/aqua/tasks/${task}`, {
    method: 'POST'
  })
  const data = await res.json()

  if (res.status === 409) {
    throw new Error('当前已有任务正在执行')
  }
  if (!res.ok || !data.ok) {
    throw new Error(data.message || '任务启动失败')
  }
  return data
}
```

## 9. 任务流程

任务启动后不是同步等待完成，而是异步执行：

```text
前端点击任务
  -> PC 后端 POST /api/aqua/tasks/feed
  -> 后端 POST 树莓派 /api/task/feed
  -> Bridge 立即返回 202 accepted
  -> Bridge 后台执行滑轨 + 机械臂任务
  -> 前端通过轮询或 WebSocket 观察 /api/status
  -> busy=false 且 phase=idle 表示完成
  -> phase=failed/error 表示失败
```

## 10. 当前机械臂初始姿态

当前已统一为：

```json
{
  "1": 220,
  "2": 489,
  "3": 130,
  "4": 842,
  "5": 836,
  "6": 509
}
```

注意：前后端一般不需要直接控制舵机，只需要展示 `servoPulse` 状态。

## 11. 联调检查清单

启动后先检查：

```bash
curl http://树莓派IP:18080/api/status
```

确认：

- `ok=true`
- `busy=false`
- `camera.hasRgb=true`
- `camera.hasDepth=true`
- RGB 视频地址能打开
- Depth 视频地址能打开

执行任务前确认：

- 不要同时发多个任务请求
- 前一个任务 `busy=false` 后再发下一个任务
- 滑轨移动时不要让机械臂执行额外动作

出问题时检查：

```bash
tail -f /tmp/aqua_bridge.log
tail -f /tmp/aqua_sdk.log
tail -f /tmp/aqua_cam.log
```

