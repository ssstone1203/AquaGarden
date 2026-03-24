#ifndef PC_CONTROL_H_
#define PC_CONTROL_H_

/**
 * @brief 初始化 PC 控制模式所需的全部外设
 *        （串口舵机、机械臂、PC 通信串口、超声波模块）
 */
void PcControl_Init(void);

/**
 * @brief 进入 PC 控制主循环（阻塞，不返回）
 *
 * 支持的文本指令（以 \n 或 \r\n 结尾）：
 *
 *   PING
 *       → 返回 "PONG\n"
 *
 *   MOVE x y z pitch min_pitch max_pitch duration
 *       → 逆运动学解算并驱动舵机；运动完成后返回 "OK\n"
 *       → 无解时立即返回 "ERR\n"
 *
 *   RESET
 *       → 机械臂复位，返回 "OK\n"
 *
 *   UNLOAD
 *       → 所有舵机卸力，返回 "OK\n"
 *
 *   READ_POS
 *       → 正向运动学计算后返回 "x,y,z,pitch\n"，失败返回 "ERR\n"
 *
 *   GRIPPER_OPEN [dur_ms]
 *       → 打开夹爪，运动完成后返回 "OK\n"（默认 500ms）
 *
 *   GRIPPER_CLOSE [dur_ms]
 *       → 关闭夹爪，运动完成后返回 "OK\n"（默认 500ms）
 *
 *   DIST
 *       → 触发超声波测距，返回 "xx.xx\n"（cm），超时返回 "ERR\n"
 *
 *   CALIB_RESET
 *       → 手眼标定：重置序号，移动到位置 01，返回实际关节角
 *
 *   1
 *       → 手眼标定：移动到下一个标定位置；全部完成后返回 "DONE\n"
 */
void PcControl_Run(void);

#endif /* PC_CONTROL_H_ */
