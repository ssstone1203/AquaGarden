# 🚀 飞腾派 Ubuntu 22.04 + ROS 2 Humble 环境搭建记录（可复用）

## 📌 一、环境说明

- 开发板：飞腾派（E2000 / D2000）
- 系统：Ubuntu 22.04
- ROS版本：ROS 2 Humble
- 架构：ARM64

------

## ⚙️ 二、基础环境准备

### 1️⃣ 设置 locale（必须）

```
sudo apt update
sudo apt install locales -y

sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8

export LANG=en_US.UTF-8
```

验证：

```
locale
```

------

### 2️⃣ 配置 Ubuntu 软件源（建议国内源）

👉 提高下载速度（可选）

```
sudo apt update
sudo apt upgrade -y
```

------

## 🌐 三、添加 ROS 2 软件源（重点）

### 1️⃣ 安装基础工具

```
sudo apt install software-properties-common curl -y
sudo add-apt-repository universe
```

------

### 2️⃣ 添加 ROS 2 GPG Key（⚠️关键）

```
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
-o /usr/share/keyrings/ros-archive-keyring.gpg
```

------

### 3️⃣ 添加软件源

```
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu jammy main" | sudo tee /etc/apt/sources.list.d/ros2.list
```

------

### 4️⃣ 更新软件源

```
sudo apt update
```

------

### ❗ 常见错误（你遇到的）

```
NO_PUBKEY F42ED6FBAB17C654
```

✔ 原因：

- 没有导入 GPG key

✔ 解决：

- 重新执行 **第2步（添加key）**

------

## 📦 四、安装 ROS 2

### 1️⃣ 安装完整版（推荐）

```
sudo apt install ros-humble-desktop -y
```

👉 如果资源紧张：

```
sudo apt install ros-humble-ros-base -y
```

------

### 2️⃣ 安装开发工具

```
sudo apt install ros-dev-tools -y
```

------

## 🔧 五、配置环境变量（必须）

### 临时生效

```
source /opt/ros/humble/setup.bash
```

### 永久生效（推荐）

```
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

------

## ✅ 六、验证安装

```
echo $ROS_DISTRO
```

输出：

```
humble
```

------

## 🧪 七、运行测试（小海龟）

### 1️⃣ 安装

```
sudo apt install ros-humble-turtlesim -y
```

------

### 2️⃣ 启动

```
ros2 run turtlesim turtlesim_node
```

------

### 3️⃣ 控制（新终端）

```
ros2 run turtlesim turtle_teleop_key
```

👉 使用方向键控制小乌龟

------

## 🧠 八、ROS 2 核心命令

### 查看节点

```
ros2 node list
```

------

### 查看包

```
ros2 pkg list
```

------

## 🏗️ 九、创建自己的 ROS 2 工程

### 1️⃣ 创建工作空间

```
mkdir -p ~/test_ws/src
cd ~/test_ws/src
```

------

### 2️⃣ 创建 Python 包

```
ros2 pkg create hello_py --build-type ament_python --dependencies rclpy
```

------

### 3️⃣ 编写节点

```
cd hello_py/hello_py
touch hello.py
```

写入代码：

```
import rclpy
from rclpy.node import Node

def main(args=None):
    rclpy.init(args=args)
    node = Node("hello_node")
    node.get_logger().info("Hello, World!")
    rclpy.spin(node)
    rclpy.shutdown()
```

------

### 4️⃣ 修改 setup.py

添加：

```
entry_points={
    'console_scripts': [
        "hello_node = hello_py.hello:main"
    ],
},
```

------

### 5️⃣ 编译

```
cd ~/test_ws
colcon build
```

------

### 6️⃣ 运行

```
source install/setup.bash
ros2 run hello_py hello_node
```

------

## 🌍 十、常见问题总结（结合你踩的坑）

### ❗ 1. GPG Key 错误

```
NO_PUBKEY
```

✔ 解决：重新导入 key

------

### ❗ 2. rosdep update 失败

```
rosdep update
```

✔ 解决（重试脚本）：

```
while true; do rosdep update && break; sleep 5; done
```

------

### ❗ 3. GitHub 下载慢 / 失败

✔ 原因：

- 网络问题（飞腾派常见）

✔ 解决：

- 多次重试 / 使用代理

------

### ❗ 4. ROS 2 节点无法运行

✔ 常见原因：

- 没 source 环境

✔ 解决：

```
source /opt/ros/humble/setup.bash
```

------

## 🧩 十一、你当前项目建议（结合 AquaGarden）

你现在的架构是：

- 树莓派 → ROS2 + Docker（机械臂）
- 飞腾派 → 传感器控制（I2C / SPI）

👉 推荐：

- 飞腾派只做：
  - 传感器采集（I2C）
  - 数据发布（ROS2 node）
- 树莓派做：
  - 控制 + 算法 + UI

------

## 🎯 十二、一句话总结

👉 飞腾派跑 ROS2 的核心就是三步：

1. **加源 + key**
2. **apt 安装 humble**
3. **source 环境**
