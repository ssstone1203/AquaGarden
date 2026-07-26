#!/bin/bash
# AquaGarden 一键启动脚本
# 用法：bash _start.sh

source /opt/ros/humble/setup.bash
source /home/ubuntu/ros2_ws/install/setup.bash
source /home/ubuntu/third_party_ros2/third_party_ws/install/setup.bash
source /home/ubuntu/AquaGarden/install/setup.bash

export CHASSIS_TYPE=Slide_Rails
export MACHINE_TYPE=ArmPi_Ultra
export need_compile=True
export ROS_DOMAIN_ID=100
export CAMERA_TYPE=aurora

# 日志目录（放在工作区内，避免 /tmp 下 root 创建文件导致当前用户 Permission denied）
LOGDIR=/home/ubuntu/AquaGarden/log
mkdir -p "$LOGDIR"
chmod 755 "$LOGDIR"
touch "$LOGDIR/aqua_sdk.log" "$LOGDIR/aqua_cam.log" "$LOGDIR/aqua_bridge.log"
# 若以 root 执行本脚本，避免日志与进程属主不一致
if [ "$(id -u)" = "0" ]; then
    chown -R ubuntu:ubuntu "$LOGDIR" 2>/dev/null || true
fi

# 停止旧节点
bash "$(dirname "$0")/_stop.sh" quiet

# 重置 ROS2 daemon，清除旧服务缓存
echo "重置 ROS2 daemon..."
ros2 daemon stop 2>/dev/null || true
sleep 1
ros2 daemon start 2>/dev/null || true
sleep 1

# 启动底层驱动（运动学 + 舵机控制）
echo "启动底层驱动..."
ros2 launch sdk armpi_ultra.launch.py > "$LOGDIR/aqua_sdk.log" 2>&1 &
echo "PID=$!  日志: $LOGDIR/aqua_sdk.log"

# SDK 一启动好舵机控制话题，立即发你的上电初始姿态。
# 不等待相机/运动学，避免机械臂长时间停在厂家 500 姿态。
echo "等待舵机控制就绪（最多 30s）..."
SERVO_ELAPSED=0
while ! ros2 topic list 2>/dev/null | grep -q "ros_robot_controller/bus_servo/set_position"; do
    sleep 1
    SERVO_ELAPSED=$((SERVO_ELAPSED + 1))
    if [ $SERVO_ELAPSED -ge 30 ]; then
        echo "错误：等待舵机控制超时，请检查 $LOGDIR/aqua_sdk.log"
        exit 1
    fi
done
echo "舵机控制就绪（${SERVO_ELAPSED}s）"

# 将机械臂移动到初始位置（1~6号舵机硬件脉冲：220 489 130 779 836 509）
echo "正在立即发送上电初始姿态..."
python3 /home/ubuntu/AquaGarden/init_arm_pose.py
echo "机械臂已进入上电初始姿态"

# 滑轨：SDK 就绪后立即物理归零（手动移动/Bridge 均依赖正确 0 点）
echo "等待滑轨 move_stepper 服务（最多 30s）..."
STEPPER_ELAPSED=0
while ! ros2 service list 2>/dev/null | grep -q "ros_robot_controller/move_stepper"; do
    sleep 1
    STEPPER_ELAPSED=$((STEPPER_ELAPSED + 1))
    if [ $STEPPER_ELAPSED -ge 30 ]; then
        echo "错误：等待滑轨服务超时，请检查 $LOGDIR/aqua_sdk.log"
        exit 1
    fi
done
echo "滑轨服务就绪（${STEPPER_ELAPSED}s）"
echo "滑轨物理归零（向近端寻限位）..."
if python3 "$(dirname "$0")/init_rail_home.py"; then
    export AQUA_RAIL_HOMED=1
    echo "滑轨已归零（近端 0）"
else
    echo "警告：滑轨归零失败，请检查 move_stepper 与 $LOGDIR/aqua_sdk.log"
fi

# 启动深度相机
echo "启动深度相机..."
ros2 launch peripherals depth_camera.launch.py > "$LOGDIR/aqua_cam.log" 2>&1 &
echo "PID=$!  日志: $LOGDIR/aqua_cam.log"

echo "等待运动学服务就绪（最多 90s）..."
TIMEOUT=90
ELAPSED=0
sleep 8
ELAPSED=8
while ! ros2 service list 2>/dev/null | grep -q "kinematics/set_pose_target"; do
    sleep 2
    ELAPSED=$((ELAPSED + 2))
    if [ $ELAPSED -ge $TIMEOUT ]; then
        echo "错误：等待运动学超时，请检查 $LOGDIR/aqua_sdk.log"
        exit 1
    fi
done
echo "运动学服务就绪（${ELAPSED}s）"

echo "等待相机话题就绪（最多 30s）..."
CAM_ELAPSED=0
while ! ros2 topic list 2>/dev/null | grep -q "depth_cam/rgb/image_raw"; do
    sleep 2
    CAM_ELAPSED=$((CAM_ELAPSED + 2))
    if [ $CAM_ELAPSED -ge 30 ]; then
        echo "警告：相机话题未就绪，请检查 $LOGDIR/aqua_cam.log"
        break
    fi
done
[ $CAM_ELAPSED -lt 30 ] && echo "相机就绪（${CAM_ELAPSED}s）"

# 启动 Web/ROS 总控桥
echo "启动 AquaGarden ROS Bridge..."
ros2 launch aqua_bridge bridge.launch.py > "$LOGDIR/aqua_bridge.log" 2>&1 &
BRIDGE_PID=$!
echo "PID=${BRIDGE_PID}  日志: $LOGDIR/aqua_bridge.log"

echo "等待 Bridge HTTP 接口就绪（最多 20s）..."
BRIDGE_ELAPSED=0
while ! python3 - <<'PY' >/dev/null 2>&1
from urllib.request import urlopen
urlopen('http://127.0.0.1:18080/api/status', timeout=1).read()
PY
do
    sleep 1
    BRIDGE_ELAPSED=$((BRIDGE_ELAPSED + 1))
    if [ $BRIDGE_ELAPSED -ge 20 ]; then
        echo "错误：Bridge 启动超时，请检查 $LOGDIR/aqua_bridge.log"
        exit 1
    fi
done
echo "Bridge 就绪（${BRIDGE_ELAPSED}s）"

echo ""
echo "所有依赖与 Web 控制接口已就绪："
echo "  状态: curl http://树莓派IP:18080/api/status"
echo "  鱼食: curl -X POST http://树莓派IP:18080/api/task/feed"
echo "  松土: curl -X POST http://树莓派IP:18080/api/task/loosen"
echo "  裁剪: curl -X POST http://树莓派IP:18080/api/task/prune"
echo "  RGB : http://树莓派IP:18080/video/rgb.mjpg"
echo "  深度: http://树莓派IP:18080/video/depth.mjpg"
