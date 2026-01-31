#ifndef __ARM_CONTROL_H__
#define __ARM_CONTROL_H__

#include "kinematics.h"
#include "serial_servo.h"
#include "global.h"

// 舵机ID定义（根据实际硬件连接）
#define SERVO_ID_BASE       1   // 基座旋转舵机（关节0）
#define SERVO_ID_SHOULDER   2   // 肩部俯仰舵机（关节1）
#define SERVO_ID_ELBOW      3   // 肘部俯仰舵机（关节2）
#define SERVO_ID_WRIST      4   // 腕部俯仰舵机（关节3）

// 舵机角度范围定义（使用global.h中的定义）
#define SERVO_MIN_POSITION  PS2_SET_MIN_DUTY      // 舵机最小位置值
#define SERVO_MAX_POSITION  PS2_SET_MAX_DUTY      // 舵机最大位置值
#define SERVO_CENTER_POSITION ((PS2_SET_MIN_DUTY + PS2_SET_MAX_DUTY) / 2)  // 舵机中位值

// 舵机控制结构体
typedef struct
{
    servo_ctrl_t servo_ctrl;       // 舵机控制对象
    kin_obj_t kin_obj;              // 运动学对象
    uint8_t initialized;            // 初始化标志
} arm_control_t;

/**
 * @brief 机械臂控制初始化
 * @param arm_ctrl 机械臂控制对象指针
 */
void ArmControl_Init(arm_control_t* arm_ctrl);

/**
 * @brief 将关节角度转换为舵机位置值
 * @param joint_angle 关节角度（度）
 * @param joint_index 关节索引（0-3）
 * @return 舵机位置值（0-1000）
 */
uint16_t ArmControl_ServoPositionFromAngle(float joint_angle, uint8_t joint_index);

/**
 * @brief 设置单个关节角度
 * @param arm_ctrl 机械臂控制对象指针
 * @param joint_index 关节索引（0-3）
 * @param angle 目标角度（度）
 * @param duration 运动时间（毫秒）
 * @return kin_status_t 状态码
 */
kin_status_t ArmControl_JointAngleSet(arm_control_t* arm_ctrl, uint8_t joint_index, float angle, uint16_t duration);

/**
 * @brief 通过逆运动学设置末端位置
 * @param arm_ctrl 机械臂控制对象指针
 * @param x 目标X坐标（cm）
 * @param y 目标Y坐标（cm）
 * @param z 目标Z坐标（cm）
 * @param pitch 目标俯仰角（度）
 * @param duration 运动时间（毫秒）
 * @return kin_status_t KIN_STATUS_OK表示成功，KIN_STATUS_INVALID表示无解
 */
kin_status_t ArmControl_EndPositionSet(arm_control_t* arm_ctrl, float x, float y, float z, float pitch, uint16_t duration);

/**
 * @brief 设置所有关节角度（直接控制）
 * @param arm_ctrl 机械臂控制对象指针
 * @param angles 4个关节的角度数组（度）
 * @param duration 运动时间（毫秒）
 */
void ArmControl_AllJointsSet(arm_control_t* arm_ctrl, float angles[4], uint16_t duration);

/**
 * @brief 机械臂复位到初始位置
 * @param arm_ctrl 机械臂控制对象指针
 * @param duration 运动时间（毫秒）
 */
void ArmControl_Reset(arm_control_t* arm_ctrl, uint16_t duration);

#endif
