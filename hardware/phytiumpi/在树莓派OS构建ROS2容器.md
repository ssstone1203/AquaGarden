# 树莓派 ROS 2 Humble 容器：构建与开发指南

面向在树莓派 OS 上使用 Docker 运行 ROS 2 Humble 的后续开发，按「先做什么、再做什么」组织。

---

## 1. 环境与约定

| 项 | 说明 |
|----|------|
| 基础镜像 | `arm64v8/ubuntu:22.04` |
| ROS 发行版 | Humble |
| 本地镜像名 | `ros2-humble:pi` |
| 工作区（宿主机） | `~/ros2_docker_ws`（可按需修改） |
| 工作区（容器内） | `/root/ros2_ws` |

镜像内已含：`ros-humble-ros-base`、`ros-dev-tools`、`colcon`、`rosdep`、`vcstool` 等（见仓库根目录 `Dockerfile`）。

---

## 2. 构建镜像（环境变更时再做）

在含 `Dockerfile` 的目录执行：

```bash
docker build -t ros2-humble:pi .
```

成功标志：日志末尾出现 `naming to docker.io/library/ros2-humble:pi`。

**何时重建**：需要把系统依赖、额外 ROS 包、工具永久写进环境时，改 `Dockerfile` 后重新执行上述命令。**日常只改业务代码不必重建。**

---

## 3. 启动容器

### 3.1 基础（仅验证或简单命令）

```bash
docker run -it --rm ros2-humble:pi
```

### 3.2 推荐：挂载工作区 + 主机网络（ROS 2 / DDS 常用）

```bash
docker run -it --rm --net=host \
  -v ~/ros2_docker_ws:/root/ros2_ws \
  ros2-humble:pi
```

- **`-v`**：代码留在宿主机，删容器不丢源码；宿主机用编辑器改，容器内编译运行。
- **`--net=host`**：与宿主机共用网络，减少 DDS 发现、组播、端口相关问题。

### 3.3 USB 摄像头

宿主机先确认设备：

```bash
ls /dev/video*
```

将设备映射进容器（可多写几个 `--device`）：

```bash
docker run -it --rm --net=host \
  --device=/dev/video0 \
  -v ~/ros2_docker_ws:/root/ros2_ws \
  ros2-humble:pi
```

优先用 **`--device=/dev/videoN`**；避免一上来就用 `--privileged` 或 `-v /dev:/dev`，除非确有需要。

---

## 4. 容器内：每次会话的 ROS 工作流

```bash
source /opt/ros/humble/setup.bash
cd /root/ros2_ws
```

首次在容器内使用 **rosdep**（镜像构建时通常未执行 `init/update`）：

```bash
rosdep init        # 容器内一般为 root；若已初始化过会报错，可忽略
rosdep update
```

编译与叠加工作区：

```bash
colcon build
source install/setup.bash
```

运行节点：

```bash
ros2 run <包名> <节点可执行名>
```

验证安装：

```bash
ros2 --help
```

---

## 5. 推荐目录布局（宿主机）

```
~/ros2_docker_ws/
├── src/          # 克隆或自建的功能包
├── Dockerfile    # 可选：与文档同仓库时已在根目录
└── ...
```

自定义包放在 `src/` 下，在容器内 `/root/ros2_ws` 执行 `colcon build`。

---

## 6. 摄像头与 ROS：分层排查

| 层次 | 检查 |
|------|------|
| 宿主机 | `ls /dev/video*` |
| 容器 | 进入容器后同样执行 `ls /dev/video*`，确认映射生效 |
| ROS 节点 | 权限、设备路径、像素格式等报错多属驱动/节点配置，不一定是 Docker 问题 |

常见节点示例（需已安装对应包）：`v4l2_camera`、`usb_cam` 等，均依赖容器内能看到 `/dev/video*`。

---

## 7. 图形界面（RViz、rqt 等）

比纯终端多一步：X11 socket、`DISPLAY` 环境变量，有时还需额外设备映射。建议先稳定 **命令行 + 采集 + Topic**；需要桌面可视化时再单独配置。

---

## 8. 心智模型（一句话）

- **镜像**：可复现的环境模板（`docker build`）。
- **容器**：从镜像跑起来的实例（`docker run`）；业务代码用挂载目录持久化。
- **宿主机**：存代码、接硬件、启动 Docker；**容器**：提供 ROS 2 与编译运行环境。

---

## 9. 速查命令

| 目的 | 命令 |
|------|------|
| 日常开发容器 | `docker run -it --rm --net=host -v ~/ros2_docker_ws:/root/ros2_ws ros2-humble:pi` |
| 带摄像头 | 在上条基础上增加 `--device=/dev/video0` |
| 进容器后环境 | `source /opt/ros/humble/setup.bash && cd /root/ros2_ws` |
| 编译 | `colcon build && source install/setup.bash` |

---

## 10. 后续可扩展方向

- 在 `Dockerfile` 中 `apt` 安装常用相机/视觉依赖，减少每次手动装包。
- 多项目可用不同镜像标签（如 `ros2-humble:proj-a`）隔离依赖版本。
- 串口、IMU 等：按需增加 `--device` 或 `udev` 规则，原则与摄像头相同：**显式映射，最小权限**。
