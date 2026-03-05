#include "arm_control.h"
#include "hal_data.h"
#include "bsp_api.h"
#include <math.h>

/* 每帧约 10 字节 @ 115200 bps ≈ 0.9 ms，帧间留 2 ms 避免覆盖 servo_frame_buf */
#define SERVO_FRAME_INTERVAL_MS  2

/**
 * @brief 机械臂控制初始化
 */
void ArmControl_Init(arm_control_t* arm_ctrl)
{
	if (arm_ctrl == NULL)
    {
        return;
    }
    // 初始化舵机控制
    Servo_Init(&arm_ctrl->servo_ctrl);
    
    // 初始化运动学对象
    Kinematics_Init(&arm_ctrl->kin_obj);
    
    // 打开串口（如果还未打开）
    // R_SCI_UART_Open(&g_serial_servo_uart_ctrl, &g_serial_servo_uart_cfg);
    
    arm_ctrl->initialized = 1;
}

/**
 * @brief 将关节角度转换为舵机位置值
 * 
 * 注意：这个函数需要根据实际舵机的安装方向和零点位置进行调整
 * 舵机位置值范围：125-875（根据global.h中的定义），对应舵机的角度范围
 * 
 * 使用复位位置作为参考点进行校准：
 * - 复位位置(15,0,2)对应的舵机位置值已定义在global.h中
 * - 通过逆运动学可以计算出复位位置对应的关节角度
 * - 基于这个对应关系进行角度到位置的转换
 * 
 * @param joint_angle 关节角度（度）
 * @param joint_index 关节索引（0-3）
 * @return 舵机位置值（125-875）
 */
uint16_t ArmControl_ServoPositionFromAngle(float joint_angle, uint8_t joint_index)
{
    uint16_t position;
    float angle_offset;
    
    // 复位位置对应的舵机位置值（从global.h）
    static const uint16_t reset_positions[4] = {
        SERIAL_SERVO1_RESET_DUTY,  // 关节0（基座）
        SERIAL_SERVO2_RESET_DUTY,  // 关节1（肩部）
        SERIAL_SERVO3_RESET_DUTY,  // 关节2（肘部）
        SERIAL_SERVO4_RESET_DUTY   // 关节3（腕部）
    };
    
    // 复位位置对应的关节角度（需要通过逆运动学计算得到，这里使用估算值）
    // 实际使用时，应该通过Kinematics_InverseKinematicsCalc计算复位位置的关节角度
    // 然后根据实际测量值调整这些角度
    static const float reset_angles[4] = {
        0.0f,   // 关节0在复位位置的角度（度）
        90.0f,  // 关节1在复位位置的角度（度）
        0.0f,   // 关节2在复位位置的角度（度）
        0.0f    // 关节3在复位位置的角度（度）
    };
    
    // 计算角度偏移量（相对于复位位置）
    angle_offset = joint_angle - reset_angles[joint_index];
    
    // 将角度偏移转换为位置偏移
    // 假设舵机角度范围是240度，对应位置范围是750（875-125）
    // 即：1度角度变化 ≈ 750/240 ≈ 3.125 位置单位
    float position_offset = angle_offset * (PS2_SET_MAX_DUTY - PS2_SET_MIN_DUTY) / 240.0f;
    
    // 计算最终位置
    position = (uint16_t)(reset_positions[joint_index] + position_offset);
    
    // 限制在有效范围内
    if (position > PS2_SET_MAX_DUTY)
    {
        position = PS2_SET_MAX_DUTY;
    }
    if (position < PS2_SET_MIN_DUTY)
    {
        position = PS2_SET_MIN_DUTY;
    }
    
    return position;
}

/**
 * @brief 设置单个关节角度
 */
kin_status_t ArmControl_JointAngleSet(arm_control_t* arm_ctrl, uint8_t joint_index, float angle, uint16_t duration)
{
    if (arm_ctrl == NULL || joint_index >= 4)
    {
        return KIN_STATUS_INVALID;
    }
    
    // 更新运动学对象中的关节角度
    arm_ctrl->kin_obj.joint[joint_index].theta = angle;
    arm_ctrl->kin_obj.joint[joint_index].rad = angle * 3.1415926f / 180.0f;
    
    // 转换为舵机位置值
    uint16_t position = ArmControl_ServoPositionFromAngle(angle, joint_index);
    
    // 确定舵机ID
    uint8_t servo_id;
    switch (joint_index)
    {
        case 0: servo_id = SERVO_ID_BASE; break;
        case 1: servo_id = SERVO_ID_SHOULDER; break;
        case 2: servo_id = SERVO_ID_ELBOW; break;
        case 3: servo_id = SERVO_ID_WRIST; break;
        default: return KIN_STATUS_INVALID;
    }
    
    // 发送舵机控制命令
    Servo_PositionSet(&arm_ctrl->servo_ctrl, servo_id, position, duration);
    
    return KIN_STATUS_OK;
}

/**
 * @brief 通过逆运动学设置末端位置
 */
kin_status_t ArmControl_EndPositionSet(arm_control_t* arm_ctrl, float x, float y, float z, float pitch, uint16_t duration)
{
    if (arm_ctrl == NULL)
    {
        return KIN_STATUS_INVALID;
    }
    
    // 设置目标位置和俯仰角
    arm_ctrl->kin_obj.vector.x = x;
    arm_ctrl->kin_obj.vector.y = y;
    arm_ctrl->kin_obj.vector.z = z;
    arm_ctrl->kin_obj.alpha_pitch = pitch;
    
    // 执行逆运动学解算
    kin_status_t status = Kinematics_InverseKinematicsCalc(&arm_ctrl->kin_obj);
    
    if (status != KIN_STATUS_OK)
    {
        return status; // 无解或超出工作空间
    }
    
    // 将解算得到的关节角度应用到舵机
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i > 0)
        {
            R_BSP_SoftwareDelay(SERVO_FRAME_INTERVAL_MS, BSP_DELAY_UNITS_MILLISECONDS);
        }
        uint16_t position = ArmControl_ServoPositionFromAngle(arm_ctrl->kin_obj.joint[i].theta, i);
        
        uint8_t servo_id;
        switch (i)
        {
            case 0: servo_id = SERVO_ID_BASE; break;
            case 1: servo_id = SERVO_ID_SHOULDER; break;
            case 2: servo_id = SERVO_ID_ELBOW; break;
            case 3: servo_id = SERVO_ID_WRIST; break;
            default: continue;
        }
        
        Servo_PositionSet(&arm_ctrl->servo_ctrl, servo_id, position, duration);
    }
    
    return KIN_STATUS_OK;
}

/**
 * @brief 设置所有关节角度（直接控制）
 */
void ArmControl_AllJointsSet(arm_control_t* arm_ctrl, float angles[4], uint16_t duration)
{
    if (arm_ctrl == NULL || angles == NULL)
    {
        return;
    }
    
    // 更新运动学对象
    for (uint8_t i = 0; i < 4; i++)
    {
        arm_ctrl->kin_obj.joint[i].theta = angles[i];
        arm_ctrl->kin_obj.joint[i].rad = angles[i] * 3.1415926f / 180.0f;
    }
    
    // 控制所有舵机
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i > 0)
        {
            R_BSP_SoftwareDelay(SERVO_FRAME_INTERVAL_MS, BSP_DELAY_UNITS_MILLISECONDS);
        }
        ArmControl_JointAngleSet(arm_ctrl, i, angles[i], duration);
    }
}

/**
 * @brief 机械臂复位到初始位置
 * 
 * 复位位置对应坐标(15, 0, 2)，舵机位置值定义在global.h中
 */
void ArmControl_Reset(arm_control_t* arm_ctrl, uint16_t duration)
{
    if (arm_ctrl == NULL)
    {
        return;
    }
    
    // 复位位置对应的舵机位置值（从global.h）
    uint8_t servo_ids[4] = {
        SERVO_ID_BASE,
        SERVO_ID_SHOULDER,
        SERVO_ID_ELBOW,
        SERVO_ID_WRIST
    };
    
    uint16_t reset_positions[4] = {
        SERIAL_SERVO1_RESET_DUTY,  // 关节0（基座）
        SERIAL_SERVO2_RESET_DUTY,  // 关节1（肩部）
        SERIAL_SERVO3_RESET_DUTY,  // 关节2（肘部）
        SERIAL_SERVO4_RESET_DUTY   // 关节3（腕部）
    };
    
    // 直接使用复位位置值控制舵机
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i > 0)
        {
            R_BSP_SoftwareDelay(SERVO_FRAME_INTERVAL_MS, BSP_DELAY_UNITS_MILLISECONDS);
        }
        Servo_PositionSet(&arm_ctrl->servo_ctrl, servo_ids[i], reset_positions[i], duration);
    }
    
    // 复位夹爪到初始位置
    Servo_PositionSet(&arm_ctrl->servo_ctrl, SERVO_ID_GRIPPER, SERIAL_SERVO5_RESET_DUTY, duration);
    
    // 更新运动学对象到复位位置
    Kinematics_Init(&arm_ctrl->kin_obj);
    // 通过逆运动学计算复位位置对应的关节角度（用于后续计算）
    Kinematics_InverseKinematicsCalc(&arm_ctrl->kin_obj);
}

void ArmControl_GripperControl(arm_control_t* arm_ctrl, bool open, uint16_t duration)
{
    if (arm_ctrl == NULL)
    {
        return;
    }
    
    // 定义夹爪打开和关闭的位置值
    // 这些值可能需要根据实际硬件进行调整
    uint16_t position;
    if (open)
    {
        // 打开夹爪：较大的位置值
        position = 700;
    }
    else
    {
        // 关闭夹爪：较小的位置值
        position = 300;
    }
    
    // 控制夹爪舵机
    Servo_PositionSet(&arm_ctrl->servo_ctrl, SERVO_ID_GRIPPER, position, duration);
}
