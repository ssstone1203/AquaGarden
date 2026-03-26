# AquaGarden 接口数据 Schema 定义文档

> 本文档定义前后端交互中所有数据结构的完整字段说明。

---

## 目录

1. [认证相关 Schema](#1-认证相关-schema)
2. [机械臂 Schema](#2-机械臂-schema)
3. [WebSocket 数据格式](#3-websocket-数据格式)
4. [前端 TypeScript 类型声明](#4-前端-typescript-类型声明)
5. [数据库模型](#5-数据库模型)

---

## 1. 认证相关 Schema

### 1.1 User

用户信息模型。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string (UUID) | 是 | 用户唯一标识 |
| `username` | string | 是 | 用户名 (3-50字符) |
| `email` | string \| null | 否 | 邮箱地址 |
| `role` | string | 是 | 角色: `admin` \| `user` \| `device` |
| `is_active` | boolean | 是 | 账户是否激活 |
| `avatar_url` | string \| null | 否 | 头像 URL |
| `bio` | string \| null | 否 | 个人简介 |
| `created_at` | string (ISO8601) | 是 | 创建时间 |
| `updated_at` | string (ISO8601) | 是 | 更新时间 |

```json
// 示例
{
    "id": "550e8400-e29b-41d4-a716-446655440000",
    "username": "admin",
    "email": "admin@example.com",
    "role": "admin",
    "is_active": true,
    "avatar_url": null,
    "bio": "系统管理员",
    "created_at": "2026-03-25T10:00:00Z",
    "updated_at": "2026-03-25T10:00:00Z"
}
```

### 1.2 RegisterRequest

注册请求。

| 字段 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `username` | string | 是 | 3-50字符，字母数字下划线连字符 | 用户名 |
| `email` | string | 否 | Email 格式 | 邮箱 |
| `password` | string | 是 | 至少6字符 | 密码 |

### 1.3 LoginRequest

登录请求。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `username` | string | 是 | 用户名 |
| `password` | string | 是 | 密码 |

### 1.4 TokenResponse

Token 响应。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `access_token` | string | 是 | JWT Access Token |
| `refresh_token` | string | 是 | JWT Refresh Token |
| `token_type` | string | 是 | 固定值: `Bearer` |
| `expires_in` | integer | 是 | Access Token 有效期 (秒) |

### 1.5 RefreshRequest

刷新 Token 请求。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `refresh_token` | string | 是 | Refresh Token |

### 1.6 ChangePasswordRequest

修改密码请求。

| 字段 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `old_password` | string | 是 | 至少6字符 | 旧密码 |
| `new_password` | string | 是 | 至少6字符 | 新密码 |

---

## 2. 机械臂 Schema

### 2.1 ArmPosition

机械臂末端位置。

| 字段 | 类型 | 单位 | 说明 |
|------|------|------|------|
| `x` | float | cm | X 坐标 |
| `y` | float | cm | Y 坐标 |
| `z` | float | cm | Z 坐标（参考桌面高度，负值表示桌面以下）|
| `pitch` | float | 度 (°) | 俯仰角 |

```json
// 示例
{
    "x": 0.0,
    "y": 15.0,
    "z": -5.0,
    "pitch": -90.0
}
```

### 2.2 ArmStatusResponse

机械臂实时状态。

| 字段 | 类型 | 说明 |
|------|------|------|
| `online` | boolean | 机械臂是否在线 |
| `position` | ArmPosition \| null | 当前位置 |
| `gripper_open` | boolean | 夹爪状态 (true=张开) |
| `calibration_loaded` | boolean | 标定是否已加载 |
| `calibration_name` | string \| null | 当前标定名称 |
| `last_command` | string \| null | 最后执行的命令 |
| `last_updated` | string \| null | 最后更新时间 (ISO8601) |

### 2.3 SerialCommandRequest

串口命令请求。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `command` | string | 是 | 命令字 |
| `params` | object \| null | 否 | 命令参数 |

**支持的命令字：**

| 命令字 | 说明 | 所需 params |
|--------|------|-------------|
| `PING` | 心跳检测 | 无 |
| `RESET` | 归零复位 | 无 |
| `UNLOAD` | 舵机卸力 | 无 |
| `READ_POS` | 读取当前位置 | 无 |
| `MOVE` | 移动到坐标 | `{x, y, z, pitch}` |
| `GRIPPER_OPEN` | 张开夹爪 | 无 |
| `GRIPPER_CLOSE` | 闭合夹爪 | 无 |
| `DIST` | 超声波测距 | 无 |

### 2.4 SerialCommandResponse

串口命令响应。

| 字段 | 类型 | 说明 |
|------|------|------|
| `command` | string | 原命令字 |
| `response` | string | 机械臂响应内容 |
| `ok` | boolean | 命令是否成功 |
| `duration_ms` | integer | 命令执行耗时 (毫秒) |
| `timestamp` | string (ISO8601) | 执行时间 |

### 2.5 CalibrationSave

保存标定请求。

| 字段 | 类型 | 必填 | 单位 | 说明 |
|------|------|------|------|------|
| `name` | string | 是 | - | 标定名称 |
| `table_z` | float | 是 | cm | 桌面高度 |
| `block_height` | float | 是 | cm | 物块高度 |
| `obs_x` | float | 是 | cm | 观测位 X |
| `obs_y` | float | 是 | cm | 观测位 Y |
| `obs_z` | float | 是 | cm | 观测位 Z |
| `obs_pitch` | float | 是 | 度 (°) | 观测位俯仰角 |
| `affine_matrix` | float[12] \| null | 否 | - | 仿射变换矩阵 (4×3，行优先) |
| `teach_samples` | object[] \| null | 否 | - | 示教原始数据 |

**affine_matrix 格式说明:**
```
[ m11, m12, m13,
  m21, m22, m23,
  m31, m32, m33,
  m41, m42, m43 ]
共12个float，对应4×3矩阵
```

### 2.6 CalibrationResponse

标定响应。

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string (UUID) | 标定记录ID |
| `name` | string | 标定名称 |
| `table_z` | float \| null | 桌面高度 |
| `block_height` | float \| null | 物块高度 |
| `obs_x` | float \| null | 观测位 X |
| `obs_y` | float \| null | 观测位 Y |
| `obs_z` | float \| null | 观测位 Z |
| `obs_pitch` | float \| null | 观测位俯仰角 |
| `is_active` | string | 是否激活: `"true"` \| `"false"` |
| `created_at` | string \| null | 创建时间 (ISO8601) |

### 2.7 ColorDetectionRequest

颜色检测请求 (Multipart Form)。

| 字段 | 类型 | 位置 | 说明 |
|------|------|------|------|
| `file` | File | Form Data | JPEG/PNG 图像文件 |
| `colors` | string | Query Param | 逗号分隔颜色列表，默认 `red,green,blue` |

**Query 参数示例:**
```
POST /api/v1/arm/detect?colors=red,green,blue
```

### 2.8 ColorTargetResult

单个颜色目标检测结果。

| 字段 | 类型 | 说明 |
|------|------|------|
| `color` | string | 颜色: `red` \| `green` \| `blue` |
| `center_x` | integer | 目标像素 X 坐标 |
| `center_y` | integer | 目标像素 Y 坐标 |
| `confidence` | float | 检测置信度 (0-1) |
| `arm_x` | float \| null | 转换后的机械臂 X (cm) |
| `arm_y` | float \| null | 转换后的机械臂 Y (cm) |
| `arm_z` | float \| null | 转换后的机械臂 Z (cm) |
| `arm_pitch` | float \| null | 转换后的俯仰角 (°) |

### 2.9 ColorDetectionResponse

颜色检测响应。

| 字段 | 类型 | 说明 |
|------|------|------|
| `targets` | ColorTargetResult[] | 检测到的颜色目标列表 |
| `obs_position` | ArmPosition \| null | 观测位位置 |

### 2.10 ClampTaskRequest

夹取分拣任务请求。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `color` | string | 是 | 目标颜色: `red` \| `green` \| `blue` |
| `use_ultrasonic` | boolean | 否 | 是否使用超声波校正，默认 `true` |

### 2.11 ClampTaskResponse

夹取分拣任务响应。

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | boolean | 任务是否成功 |
| `color` | string | 目标颜色 |
| `target_position` | ArmPosition | 目标放置位置 |
| `duration_seconds` | float | 任务执行耗时 (秒) |
| `steps` | string[] | 执行步骤列表 (共12步) |

**执行步骤列表:**
```json
[
    "移到观测位",
    "颜色检测定位",
    "移到目标上方",
    "超声波X轴校正",
    "张开夹爪",
    "下降到夹取高度",
    "闭合夹爪",
    "抬起到安全高度",
    "移到分拣区正上方",
    "下降到放置高度",
    "松开夹爪",
    "归零"
]
```

### 2.12 OperationLogEntry

操作日志条目。

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | 日志ID |
| `action` | string | 操作类型 (命令字) |
| `params` | object \| null | 操作参数 |
| `response` | string \| null | 响应内容 |
| `result` | string | 结果: `ok` \| `timeout` \| `error` |
| `duration_ms` | integer \| null | 执行耗时 (毫秒) |
| `created_at` | string (ISO8601) | 操作时间 |

### 2.13 PaginatedData<T>

通用分页数据。

| 字段 | 类型 | 说明 |
|------|------|------|
| `total` | integer | 总记录数 |
| `page` | integer | 当前页码 |
| `page_size` | integer | 每页数量 |
| `total_pages` | integer | 总页数 |
| `data` | T[] | 数据列表 |

### 2.14 ErrorResponse

统一错误响应。

| 字段 | 类型 | 说明 |
|------|------|------|
| `error` | ErrorDetail | 错误详情 |

**ErrorDetail:**

| 字段 | 类型 | 说明 |
|------|------|------|
| `code` | string | 错误代码 |
| `message` | string | 错误信息 |
| `details` | object \| null | 详细信息 |
| `request_id` | string \| null | 请求追踪ID |

---

## 3. WebSocket 数据格式

### 3.1 机械臂状态推送 (每 2 秒)

**服务端 → 客户端**

```json
{
    "type": "arm_status",
    "online": true,
    "position": {
        "x": 0.0,
        "y": 15.0,
        "z": -5.0,
        "pitch": -90.0
    },
    "gripper_open": true,
    "calibration_loaded": true,
    "timestamp": "2026-03-25T10:05:00.000Z"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定值: `arm_status` |
| `online` | boolean | 机械臂在线状态 |
| `position` | object | 当前位置 |
| `gripper_open` | boolean | 夹爪状态 |
| `calibration_loaded` | boolean | 标定加载状态 |
| `timestamp` | string | 服务器时间戳 |

### 3.2 标定通道 Ping

**客户端 → 服务端**

```json
{ "type": "ping" }
```

**服务端 → 客户端**

```json
{
    "type": "pong",
    "calibration_loaded": true,
    "timestamp": "2026-03-25T10:05:00.000Z"
}
```

### 3.3 连接参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `token` | string (Query) | JWT Access Token (可选，建议填写) |

**示例:**
```
ws://localhost:8000/api/v1/ws/arm/status?token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
ws://localhost:8000/api/v1/ws/arm/calibration?token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

---

## 4. 前端 TypeScript 类型声明

```typescript
// ── 认证 ─────────────────────────────────────────────────────────────────────

interface User {
    id: string;
    username: string;
    email: string | null;
    role: 'admin' | 'user' | 'device';
    is_active: boolean;
    avatar_url: string | null;
    bio: string | null;
    created_at: string;
    updated_at: string;
}

interface LoginRequest {
    username: string;
    password: string;
}

interface RegisterRequest {
    username: string;
    password: string;
    email?: string;
}

interface TokenResponse {
    access_token: string;
    refresh_token: string;
    token_type: string;
    expires_in: number;
}

interface RefreshRequest {
    refresh_token: string;
}

interface ChangePasswordRequest {
    old_password: string;
    new_password: string;
}

// ── 机械臂 ───────────────────────────────────────────────────────────────────

interface ArmPosition {
    x: number;
    y: number;
    z: number;
    pitch: number;
}

interface ArmStatusResponse {
    online: boolean;
    position: ArmPosition | null;
    gripper_open: boolean;
    calibration_loaded: boolean;
    calibration_name: string | null;
    last_command: string | null;
    last_updated: string | null;
}

interface SerialCommandRequest {
    command: 'PING' | 'RESET' | 'UNLOAD' | 'READ_POS' | 'MOVE'
            | 'GRIPPER_OPEN' | 'GRIPPER_CLOSE' | 'DIST';
    params?: { x?: number; y?: number; z?: number; pitch?: number } | null;
}

interface SerialCommandResponse {
    command: string;
    response: string;
    ok: boolean;
    duration_ms: number;
    timestamp: string;
}

interface CalibrationSave {
    name: string;
    table_z: number;
    block_height: number;
    obs_x: number;
    obs_y: number;
    obs_z: number;
    obs_pitch: number;
    affine_matrix?: number[] | null;
    teach_samples?: Record<string, any>[] | null;
}

interface CalibrationResponse {
    id: string;
    name: string;
    table_z: number | null;
    block_height: number | null;
    obs_x: number | null;
    obs_y: number | null;
    obs_z: number | null;
    obs_pitch: number | null;
    is_active: string;
    created_at: string | null;
}

interface ColorTargetResult {
    color: 'red' | 'green' | 'blue';
    center_x: number;
    center_y: number;
    confidence: number;
    arm_x: number | null;
    arm_y: number | null;
    arm_z: number | null;
    arm_pitch: number | null;
}

interface ColorDetectionResponse {
    targets: ColorTargetResult[];
    obs_position: ArmPosition | null;
}

interface ClampTaskRequest {
    color: 'red' | 'green' | 'blue';
    use_ultrasonic?: boolean;
}

interface ClampTaskResponse {
    success: boolean;
    color: string;
    target_position: ArmPosition;
    duration_seconds: number;
    steps: string[];
}

interface OperationLogEntry {
    id: number;
    action: string;
    params: Record<string, any> | null;
    response: string | null;
    result: 'ok' | 'timeout' | 'error';
    duration_ms: number | null;
    created_at: string;
}

interface PaginatedData<T> {
    total: number;
    page: number;
    page_size: number;
    total_pages: number;
    data: T[];
}

// ── WebSocket ────────────────────────────────────────────────────────────────

interface WSArmStatusData {
    type: 'arm_status';
    online: boolean;
    position: ArmPosition;
    gripper_open: boolean;
    calibration_loaded: boolean;
    timestamp: string;
}

interface WSCalibrationPong {
    type: 'pong';
    calibration_loaded: boolean;
    timestamp: string;
}

// ── 错误 ─────────────────────────────────────────────────────────────────────

interface ErrorDetail {
    code: string;
    message: string;
    details?: Record<string, any>;
    request_id?: string;
}

interface ErrorResponse {
    error: ErrorDetail;
}
```

---

## 5. 数据库模型

### 5.1 users 表

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| `id` | UUID | PK | 用户ID |
| `username` | VARCHAR(50) | UNIQUE, INDEX | 用户名 |
| `email` | VARCHAR(255) | UNIQUE, NULL, INDEX | 邮箱 |
| `hashed_password` | VARCHAR(255) | NOT NULL | 密码哈希 |
| `role` | VARCHAR(20) | DEFAULT 'user' | 角色 |
| `is_active` | BOOLEAN | DEFAULT TRUE | 是否激活 |
| `avatar_url` | TEXT | NULL | 头像URL |
| `bio` | TEXT | NULL | 个人简介 |
| `created_at` | TIMESTAMP | NOT NULL | 创建时间 |
| `updated_at` | TIMESTAMP | NOT NULL | 更新时间 |

### 5.2 robot_calibrations 表

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| `id` | UUID | PK | 标定ID |
| `name` | VARCHAR(100) | UNIQUE, INDEX | 标定名称 |
| `affine_matrix` | FLOAT[] | NULL | 仿射变换矩阵 |
| `obs_x` | FLOAT | NULL | 观测位 X |
| `obs_y` | FLOAT | NULL | 观测位 Y |
| `obs_z` | FLOAT | NULL | 观测位 Z |
| `obs_pitch` | FLOAT | NULL | 观测位俯仰角 |
| `table_z` | FLOAT | NULL | 桌面高度 |
| `block_height` | FLOAT | NULL | 物块高度 |
| `teach_samples` | JSONB | NULL | 示教原始数据 |
| `is_active` | VARCHAR(10) | DEFAULT 'false' | 是否激活 |
| `created_at` | TIMESTAMP | NOT NULL | 创建时间 |
| `updated_at` | TIMESTAMP | NOT NULL | 更新时间 |

### 5.3 robot_operation_logs 表

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| `id` | SERIAL | PK | 日志ID |
| `action` | VARCHAR(50) | INDEX | 操作类型 |
| `params` | JSONB | NULL | 操作参数 |
| `response` | TEXT | NULL | 响应内容 |
| `result` | VARCHAR(20) | NOT NULL | 执行结果 |
| `duration_ms` | INTEGER | NULL | 耗时 (毫秒) |
| `created_at` | TIMESTAMP | NOT NULL | 操作时间 |

---

## 6. 分拣区域坐标参考

### 6.1 预定义分拣区坐标 (后端预设)

| 颜色 | X (cm) | Y (cm) | Z (cm) | Pitch (°) | 说明 |
|------|--------|--------|--------|-----------|------|
| `red` | 8.31 | 21.78 | -6.54 | -74.2 | 红色物块分拣区 |
| `green` | 0.84 | 22.21 | -7.01 | -78.0 | 绿色物块分拣区 |
| `blue` | -4.36 | 22.34 | -7.24 | -72.2 | 蓝色物块分拣区 |

### 6.2 坐标系说明

```
        Y轴 (前方)
          ↑
          |
          |
          |
          +──────────→ X轴 (右方)
         /
        /
       ↓
    Z轴 (下方/高度)
```

- **X**: 水平左右，正值为右
- **Y**: 水平前后，正值为前
- **Z**: 垂直高度，0为参考平面，负值表示低于参考面
- **Pitch**: 俯仰角，正值抬头，负值低头

### 6.3 坐标系标定参考

| 标定项 | 典型值 | 说明 |
|--------|--------|------|
| `table_z` | -8.5 cm | 桌面高度 |
| `block_height` | 3.0 cm | 物块厚度 |
| `obs_x` | 0.0 cm | 摄像头正下方X |
| `obs_y` | 15.0 cm | 摄像头正下方Y |
| `obs_z` | -5.0 cm | 摄像头高度 |
| `obs_pitch` | -90.0 ° | 俯视角度 |
