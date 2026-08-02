# AquaGarden FastAPI + Vue Conda 环境配置指南

本文档适用于在 Windows PowerShell 中配置和运行以下两个项目：

- FastAPI 后端：`backend-fastapi`
- Vue 3 前端：`front-vue`

推荐创建一个名为 `aquagarden` 的 Conda 环境，同时安装 Python 和 Node.js。Python 依赖仍由 `pip` 根据 `requirements.txt` 安装，Vue 依赖仍由 `npm` 根据 `package-lock.json` 安装。Conda 负责隔离 Python、Node.js、pip 和 npm 的版本。

## 1. 环境要求

### 1.1 推荐版本

| 组件 | 推荐版本 | 项目依据 |
| --- | --- | --- |
| 操作系统 | Windows 10/11 64 位 | 项目默认使用 `COM20` 串口，并直接访问 USB 摄像头 |
| Conda | Miniconda/Anaconda 24.x 或更高 | 用于创建隔离环境 |
| Python | 3.11.x | 兼容 FastAPI、OpenCV、Ultralytics 等后端依赖 |
| Node.js | 22.x LTS | Vite 8 要求 `^20.19.0` 或 `>=22.12.0` |
| npm | 随 Node.js 22 安装 | 项目使用 `package-lock.json` 锁定前端依赖 |
| 内存 | 最低 8 GB，推荐 16 GB | Ultralytics、PyTorch 和视频识别占用较多内存 |
| 可用磁盘 | 建议至少 10 GB | Conda、PyTorch、Ultralytics、OpenCV 和 `node_modules` 体积较大 |

> 注意：Node.js 18 不满足当前 `vite@8` 的运行要求。本项目应使用 Node.js 20.19+ 或 Node.js 22.12+，推荐 Node.js 22 LTS。

### 1.2 本机服务端口

| 服务 | 默认地址 |
| --- | --- |
| FastAPI | `http://127.0.0.1:8090` |
| FastAPI Swagger 文档 | `http://127.0.0.1:8090/docs` |
| Vue/Vite | `http://127.0.0.1:5173` |

Vite 已将 `/api` 和 `/ws` 代理到 `http://localhost:8090`，因此联调时应先启动 FastAPI，再启动 Vue。

### 1.3 可选硬件要求

只运行页面和接口时不要求连接硬件。使用完整功能时还需要：

- MCU 对应的 USB 串口驱动，默认串口为 `COM20`，波特率为 `115200`。
- USB 摄像头，默认摄像头索引由 `backend-fastapi/.env` 配置。
- 使用 NVIDIA GPU 加速 YOLO 时，需要正确安装显卡驱动，并安装与驱动兼容的 PyTorch/CUDA 版本。CPU 也可以运行，但识别速度较慢。

## 2. 安装并初始化 Conda

当前机器如果已经可以执行 `conda --version`，直接跳到第 3 节。

### 2.1 使用 winget 安装 Miniconda

```powershell
winget install --exact --id Anaconda.Miniconda3
```

安装完成后关闭并重新打开 PowerShell，然后初始化 Conda：

```powershell
conda init powershell
```

再次关闭并打开 PowerShell，确认安装成功：

```powershell
conda --version
conda info --envs
```

也可以从 Miniconda 官方页面下载安装程序：<https://docs.conda.io/projects/miniconda/en/latest/>

## 3. 创建 Conda 环境

在任意目录执行：

```powershell
conda create --name aquagarden --channel conda-forge python=3.11 nodejs=22 pip -y
conda activate aquagarden
```

验证当前终端使用的工具都来自新环境：

```powershell
python --version
node --version
npm --version
python -m pip --version
where.exe python
where.exe node
```

预期结果：

- Python 为 `3.11.x`。
- Node.js 为 `v22.x`，且版本不低于 `22.12.0`。
- `python` 和 `node` 的路径位于 Conda 的 `aquagarden` 环境目录中。

如果已存在同名环境，不要重复创建，直接执行：

```powershell
conda activate aquagarden
```

## 4. 安装 FastAPI 后端依赖

进入本仓库和后端目录。请根据本机实际仓库位置调整第一行路径：

```powershell
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm"
conda activate aquagarden
Set-Location .\backend-fastapi

python -m pip install --upgrade pip setuptools wheel
python -m pip install -r requirements.txt
```

后端主要依赖包括：

- FastAPI `0.115.6` 和 Uvicorn `0.34.0`
- SQLAlchemy、Pydantic Settings、JWT 和 bcrypt
- OpenCV、Pillow、Ultralytics 和 PyTorch 相关依赖
- PySerial，用于 MCU 串口通信

安装过程会下载较大的 PyTorch、OpenCV 和 Ultralytics 包，耗时较长属于正常现象。

验证后端依赖：

```powershell
python -c "import fastapi, uvicorn, cv2, serial, ultralytics; print('FastAPI dependencies OK')"
python -m pip check
```

## 5. 配置后端环境变量

后端从当前工作目录下的 `.env` 读取配置，因此必须在 `backend-fastapi` 目录中启动。

仓库中已经存在 `.env` 时先保留现有配置；没有时，从示例文件创建：

```powershell
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm\backend-fastapi"

if (-not (Test-Path .env)) {
    Copy-Item .env.example .env
}
```

至少检查以下配置：

```dotenv
AQUAGARDEN_FASTAPI_PORT=8090
AQUAGARDEN_HARDWARE_SERIAL_ENABLED=true
AQUAGARDEN_HARDWARE_SERIAL_PORT=COM20
AQUAGARDEN_HARDWARE_SERIAL_BAUD=115200
AQUAGARDEN_TANK_USB_CAMERA_ENABLED=true
AQUAGARDEN_TANK_USB_CAMERA_INDEX=0
```

注意事项：

- 实际串口不是 `COM20` 时，修改 `AQUAGARDEN_HARDWARE_SERIAL_PORT`。
- 实际摄像头不是索引 `0` 时，修改 `AQUAGARDEN_TANK_USB_CAMERA_INDEX`。
- `AQUAGARDEN_JWT_SECRET`、管理员密码和 LLM API Key 不要提交到 Git。
- 不使用 AI 功能时可以将 `AQUAGARDEN_LLM_ENABLED=false`。

首次只验证软件环境、暂时不连接串口和摄像头时，可以在当前 PowerShell 会话中临时禁用硬件：

```powershell
$env:AQUAGARDEN_HARDWARE_SERIAL_ENABLED="false"
$env:AQUAGARDEN_TANK_USB_CAMERA_ENABLED="false"
$env:AQUAGARDEN_TANK_YOLO_ENABLED="false"
$env:AQUAGARDEN_LLM_ENABLED="false"
```

这些临时变量只对当前 PowerShell 窗口有效，关闭窗口后失效。

## 6. 安装 Vue 前端依赖

在仓库的前端目录执行：

```powershell
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm\front-vue"
conda activate aquagarden
npm ci
```

`npm ci` 会严格按照 `package-lock.json` 安装依赖，适合首次配置和可复现安装。如果已经修改了 `package.json` 并需要更新锁文件，则改用：

```powershell
npm install
```

验证前端可以构建：

```powershell
npm run build
```

## 7. 启动项目

前后端需要分别占用一个终端。两个终端都要激活同一个 Conda 环境。

### 7.1 终端一：启动 FastAPI

```powershell
conda activate aquagarden
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm\backend-fastapi"
python main.py
```

需要开发热更新时也可以执行：

```powershell
python -m uvicorn app.main:app --host 0.0.0.0 --port 8090 --reload
```

后端启动后访问：

- API 文档：<http://127.0.0.1:8090/docs>
- 串口状态：<http://127.0.0.1:8090/api/mcu/serial/status>
- 传感器数据：<http://127.0.0.1:8090/api/sensors>

PowerShell 健康检查命令：

```powershell
Invoke-RestMethod http://127.0.0.1:8090/api/sensors
```

### 7.2 终端二：启动 Vue

```powershell
conda activate aquagarden
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm\front-vue"
npm run dev
```

浏览器访问：<http://127.0.0.1:5173>

终止服务时，在各自终端按 `Ctrl+C`。

## 8. 完整验证

### 8.1 后端测试

```powershell
conda activate aquagarden
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm\backend-fastapi"
python -m pytest -q
```

如果提示没有安装 `pytest`，先安装开发测试依赖：

```powershell
python -m pip install pytest
python -m pytest -q
```

### 8.2 前端测试与构建

```powershell
conda activate aquagarden
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm\front-vue"
npm run test:controls
npm run build
```

## 9. 常见问题

### 9.1 PowerShell 找不到 `conda`

先尝试从“Anaconda Prompt”执行：

```powershell
conda init powershell
```

关闭所有 PowerShell 窗口后重新打开。如果 PowerShell 阻止加载配置脚本，可检查执行策略：

```powershell
Get-ExecutionPolicy -List
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

执行策略属于系统安全设置，只在确认符合本机安全要求后修改。

### 9.2 激活环境后版本仍然不正确

```powershell
conda activate aquagarden
where.exe python
where.exe node
Get-Command python
Get-Command node
```

路径首项应位于 `aquagarden` 环境中。若仍指向系统 Node.js，关闭终端并重新打开后再激活环境。

### 9.3 Vite 报 Node.js 版本不支持

确认 Node.js 不低于 `22.12.0`：

```powershell
node --version
conda install --name aquagarden --channel conda-forge nodejs=22 -y
```

然后重新激活环境并安装前端依赖：

```powershell
conda activate aquagarden
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm\front-vue"
npm ci
```

### 9.4 端口 8090 或 5173 被占用

查看端口占用进程：

```powershell
Get-NetTCPConnection -LocalPort 8090,5173 -ErrorAction SilentlyContinue |
    Select-Object LocalAddress, LocalPort, State, OwningProcess
```

关闭已占用端口的旧服务，或修改后端端口和 `front-vue/vite.config.js` 中对应的代理端口。前后端端口必须保持一致。

### 9.5 串口连接失败

列出当前可用串口：

```powershell
conda activate aquagarden
python -m serial.tools.list_ports -v
```

将结果中的正确端口写入 `backend-fastapi/.env`。还要确认串口没有被串口调试工具或其他进程占用。

### 9.6 摄像头或 YOLO 启动失败

先关闭正在占用摄像头的浏览器、会议软件或采集软件。只验证 API 时可临时禁用摄像头和 YOLO：

```powershell
$env:AQUAGARDEN_TANK_USB_CAMERA_ENABLED="false"
$env:AQUAGARDEN_TANK_YOLO_ENABLED="false"
python main.py
```

查看 PyTorch 是否识别 NVIDIA GPU：

```powershell
python -c "import torch; print('torch:', torch.__version__); print('CUDA available:', torch.cuda.is_available())"
```

`CUDA available: False` 不影响 CPU 运行。确实需要 GPU 加速时，应按照 PyTorch 官方安装选择器生成与本机驱动匹配的命令：<https://pytorch.org/get-started/locally/>

### 9.7 下载速度慢或安装中断

重新激活环境后重复执行失败的安装命令即可。先确认能够访问 Conda 和 npm 软件源，不建议混用多个来源不明的镜像，以免得到不一致或过期的二进制包。

## 10. 日常启动速查

后端终端：

```powershell
conda activate aquagarden
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm\backend-fastapi"
python main.py
```

前端终端：

```powershell
conda activate aquagarden
Set-Location "E:\code\fish-arm\AquaGarden\software\fish-arm\front-vue"
npm run dev
```

环境退出：

```powershell
conda deactivate
```
