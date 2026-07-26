#!/usr/bin/env python3
"""上电后将机械臂移动到默认初始位置"""
import rclpy
from rclpy.node import Node
from ros_robot_controller_msgs.msg import ServosPosition, ServoPosition
import time

# 初始位置硬件脉冲值  舵机 1~6。4 号更换后需要 -63 脉冲补偿。
INIT_PULSES = {1: 220, 2: 489, 3: 130, 4: 779, 5: 836, 6: 509}
DURATION_MS = 1500  # 移动时间 ms

class InitArmPose(Node):
    def __init__(self):
        super().__init__('init_arm_pose')
        self.pub = self.create_publisher(
            ServosPosition,
            '/ros_robot_controller/bus_servo/set_position',
            1
        )
        # _start.sh 已经等到舵机控制话题出现；这里再短暂等待订阅者连接。
        deadline = time.time() + 2.0
        while self.pub.get_subscription_count() == 0 and time.time() < deadline:
            time.sleep(0.05)
        self._send()

    def _send(self):
        msg = ServosPosition()
        msg.duration = float(DURATION_MS) / 1000.0
        for sid, pos in INIT_PULSES.items():
            sp = ServoPosition()
            sp.id = sid
            sp.position = pos
            msg.position.append(sp)
        # 连续发布两次，避免刚启动时第一帧被 DDS 发现过程吞掉。
        self.pub.publish(msg)
        time.sleep(0.1)
        self.pub.publish(msg)
        self.get_logger().info(f'初始位姿已发送: {INIT_PULSES}  ({DURATION_MS}ms)')
        time.sleep(DURATION_MS / 1000.0 + 0.5)

def main():
    rclpy.init()
    node = InitArmPose()
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
