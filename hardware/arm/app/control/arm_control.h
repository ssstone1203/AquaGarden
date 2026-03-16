#ifndef __ARM_CONTROL_H__
#define __ARM_CONTROL_H__

#include "kinematics.h"
#include "serial_servo.h"
#include "global.h"

// 舵机数量定义（移植自 LeArm MAX_SERVOS_NUM）
#define ARM_MAX_SERVOS_NUM                          6

// 舵机ID定义（与LeArm硬件保持一致）
// LeArm ID分配：1=夹爪, 2=旋转, 3=腕部俯仰, 4=肘部俯仰, 5=肩部俯仰, 6=基座旋转
#define SERVO_ID_GRIPPER    1   // 夹爪舵机（ID 1）
#define SERVO_ID_ROTATION   2   // 旋转舵机（ID 2，腕部旋转）
#define SERVO_ID_WRIST      3   // 腕部俯仰舵机（ID 3，关节3）
#define SERVO_ID_ELBOW      4   // 肘部俯仰舵机（ID 4，关节2）
#define SERVO_ID_SHOULDER   5   // 肩部俯仰舵机（ID 5，关节1）
#define SERVO_ID_BASE       6   // 基座旋转舵机（ID 6，关节0）

// 舵机角度范围定义（使用global.h中的定义）
#define SERVO_MIN_POSITION  PS2_SET_MIN_DUTY      // 舵机最小位置值
#define SERVO_MAX_POSITION  PS2_SET_MAX_DUTY      // 舵机最大位置值
#define SERVO_CENTER_POSITION ((PS2_SET_MIN_DUTY + PS2_SET_MAX_DUTY) / 2)  // 舵机中位值

// 串口舵机角度转换因子（移植自 LeArm SERIAL_ANGLE_FACTOR）
// 舵机位置值范围125~875对应240°机械行程，1° ≈ 3.125位置单位
#define SERIAL_ANGLE_FACTOR                     4.166666666666667f

// 默认坐标位置（移植自 LeArm DEFAULT_X/Y/Z）
#define ARM_DEFAULT_X                           15.0f
#define ARM_DEFAULT_Y                            0.0f
#define ARM_DEFAULT_Z                            2.0f

// 坐标范围限制（移植自 LeArm MAX/MIN_X/Y/Z）
#define ARM_MAX_X                               20.0f
#define ARM_MIN_X                               10.0f
#define ARM_MAX_Y                               10.0f
#define ARM_MIN_Y                              -10.0f
#define ARM_MAX_Z                               25.0f
#define ARM_MIN_Z                                0.0f

// 夹爪角度定义（移植自 LeArm）
#define ARM_DEFAULT_CLAW_OPEN_ANGLE             90.0f
#define ARM_CLAW_MIN_OPEN_ANGLE                  0.0f
#define ARM_CLAW_MAX_OPEN_ANGLE                 90.0f

// 腕部旋转角度定义（移植自 LeArm）
#define ARM_DEFAULT_ROTATION_ANGLE              90.0f
#define ARM_ROTATION_MIN_ANGLE                 -90.0f
#define ARM_ROTATION_MAX_ANGLE                  90.0f

// 舵机控制结构体
typedef struct
{
    servo_ctrl_t servo_ctrl;       // 舵机控制对象
    kin_obj_t kin_obj;              // 运动学对象
    uint8_t initialized;            // 初始化标志
} arm_control_t;

/**
 * @brief 机械臂控制初始化
 */
bool ArmControl_Init(void);

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

/**
 * @brief 控制夹爪开合
 * @param arm_ctrl 机械臂控制对象指针
 * @param open true表示打开夹爪，false表示关闭夹爪
 * @param duration 运动时间（毫秒）
 */
void ArmControl_GripperControl(arm_control_t* arm_ctrl, bool open, uint16_t duration);

//末端坐标控制
uint8_t ArmControl_CoordinateSet(float target_x, float target_y, float target_z, 
								 float pitch, float min_pitch, float max_pitch,
								 uint16_t time);

#endif
