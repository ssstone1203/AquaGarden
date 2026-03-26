# AquaGarden 机械臂控制系统后端

基于 FastAPI 构建的机械臂智能控制系统后端 API，与 RA6M5 固件串口协议无缝对接。

## 技术栈

- **框架**: FastAPI 0.109 + Uvicorn
- **数据库**: PostgreSQL 15 + SQLAlchemy 2.0 (异步)
- **缓存**: Redis 7
- **认证**: JWT (HS256)
- **通信**: PySerial（115200 波特率，RA6M5 串口协议）

## 快速开始

```bash
# 1. 安装依赖
pip install -r requirements.txt

# 2. 配置环境变量
cp .env.example .env
# 编辑 .env 填入实际串口和数据库配置

# 3. 启动服务
uvicorn app.main:app --reload --port 8000

# 4. 访问 API 文档
# Swagger UI: http://localhost:8000/docs
# ReDoc: http://localhost:8000/redoc
```

## 项目结构

```
backend/
├── app/
│   ├── main.py              # FastAPI 应用入口
│   ├── config.py            # 全局配置
│   ├── database.py           # PostgreSQL 异步连接
│   ├── redis_client.py      # Redis 连接
│   ├── deps.py              # 依赖注入
│   ├── core/
│   │   ├── security.py     # JWT + bcrypt
│   │   └── rbac.py         # 权限控制
│   ├── models/
│   │   ├── user.py         # 用户表
│   │   └── robot.py        # 标定数据表 + 操作日志表
│   ├── schemas/
│   │   ├── common.py       # 分页响应
│   │   ├── auth.py         # 认证模型
│   │   └── robot.py       # 机械臂相关模型
│   ├── services/
│   │   ├── auth_service.py  # 认证服务
│   │   ├── robot_service.py # 机械臂控制 + 串口协议
│   │   └── color_service.py # 颜色检测 + 坐标转换
│   ├── api/v1/
│   │   ├── auth.py        # /api/v1/auth
│   │   ├── robot.py      # /api/v1/arm
│   │   └── websocket.py  # /api/v1/ws
│   └── ml/
│       └── color_detector.py  # OpenCV HSV 颜色检测
├── requirements.txt
└── README.md
```

## API 概览

### 认证 `/api/v1/auth`

| 方法 | 路径 | 功能 |
|------|------|------|
| POST | /register | 用户注册 |
| POST | /login | 登录，返回 JWT |
| POST | /logout | 退出登录 |
| POST | /refresh | 刷新 Token |
| GET | /me | 获取当前用户 |

### 机械臂 `/api/v1/arm`

| 方法 | 路径 | 功能 |
|------|------|------|
| POST | /connect | 建立 RA6M5 串口连接 |
| POST | /disconnect | 断开串口连接 |
| GET | /status | 获取机械臂实时状态 |
| POST | /command | 发送串口命令 |
| GET | /calibrations | 获取所有标定数据 |
| POST | /calibrations | 保存标定数据 |
| POST | /calibrations/{name}/load | 加载标定到内存 |
| POST | /detect | 上传图像检测颜色并转换为机械臂坐标 |
| POST | /clamp | 执行彩色物块夹取分拣任务 |
| GET | /logs | 获取操作日志 |

### WebSocket `/api/v1/ws`

| 路径 | 功能 |
|------|------|
| /ws/arm/status | 机械臂状态实时推送（每 2 秒） |
| /ws/arm/calibration | 标定数据实时同步通道 |

## 串口命令协议

对应 RA6M5 固件 `serial_protocol.c` 中定义的协议：

| 命令 | 说明 | 参数格式 |
|------|------|----------|
| PING | 心跳检测 | 无 |
| RESET | 归零复位 | 无 |
| UNLOAD | 舵机卸力 | 无 |
| READ_POS | 读取当前位置 | 无，响应 `POS:x,y,z,pitch` |
| MOVE | 移动到坐标 | `MOVE:x,y,z,pitch` |
| GRIPPER_OPEN | 张开夹爪 | 无 |
| GRIPPER_CLOSE | 闭合夹爪 | 无 |
| DIST | 超声波测距 | 无，响应 `DIST:distance` |

## 标定流程

1. **z 轴标定** — 使用 `teach.py` 确定桌面高度（TABLE_Z）和物块高度（BLOCK_HEIGHT=3cm）
2. **示教采集** — 使用 `collect_teach.py` 在观测位姿采集 9 组数据，拟合 4×3 仿射变换矩阵
3. **数据保存** — 调用 `POST /api/v1/arm/calibrations` 将标定数据存入数据库
4. **加载使用** — 调用 `POST /api/v1/arm/calibrations/{name}/load` 加载到内存

## 夹取分拣流程

1. 摄像头俯视桌面 → `POST /detect` 检测目标像素坐标
2. 像素坐标通过仿射矩阵 → 转换为机械臂坐标
3. 超声波测距 → X 轴校正
4. 执行 12 步夹取序列 → 分拣区放置
5. 操作全程记录到操作日志表
