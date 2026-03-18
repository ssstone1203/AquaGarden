#include "arm_control.h"
#include "hal_data.h"
#include "bsp_api.h"
#include <math.h>

/* 每帧约 10 字节 @ 9600 bps ≈ 10.4 ms，帧间留 20 ms 避免覆盖 servo_frame_buf */
#define SERVO_FRAME_INTERVAL_MS  30

kin_obj_t g_kin_obj;
arm_control_t g_arm_ctrl;

static void Theta_To_Servo(kin_obj_t* kin_obj, uint16_t time)
{
	float target_angle[4] = {0};

	target_angle[0] = kin_obj->joint[0].theta;
	target_angle[1] = 90.0f - kin_obj->joint[1].theta;
	target_angle[2] = kin_obj->joint[2].theta;
	target_angle[3] = kin_obj->joint[3].theta;
	for (uint8_t i = 0; i < 4; i++)
	{	
		Servo_PositionSet(&g_servo_ctrl, 6 - i, 500 + (SERIAL_ANGLE_FACTOR * target_angle[i]), time);
//		serial_servo_set_position(&serial_servo_controller, 6 - i, 500 + (int)(SERIAL_ANGLE_FACTOR * target_angle[i]), time);
		R_BSP_SoftwareDelay(SERVO_FRAME_INTERVAL_MS, BSP_DELAY_UNITS_MILLISECONDS);
	}
}


/**
 * @brief 机械臂控制初始化
 */
bool ArmControl_Init(void)
{
//	int8_t read_offset[6];
	
	Servo_Init(&g_servo_ctrl);
	
	Kin_Init(&g_kin_obj);
	
//	kinematics_init(&kinematics);
	memset(&g_arm_ctrl, 0, sizeof(arm_control_t));
	
	R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
	ArmControl_Reset(&g_arm_ctrl, 2000);
	
//	robot_arm_offset_read(read_offset);
	return true;
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
    
    // 复位位置对应的舵机位置值（与LeArm ID映射保持一致）
    // 关节0(基座)→ID6, 关节1(肩部)→ID5, 关节2(肘部)→ID4, 关节3(腕部)→ID3
    static const uint16_t reset_positions[4] = {
        SERIAL_SERVO6_RESET_DUTY,  // 关节0（基座，ID 6）
        SERIAL_SERVO5_RESET_DUTY,  // 关节1（肩部，ID 5）
        SERIAL_SERVO4_RESET_DUTY,  // 关节2（肘部，ID 4）
        SERIAL_SERVO3_RESET_DUTY   // 关节3（腕部，ID 3）
    };
    
    // 复位位置对应的关节角度（根据LeArm坐标系反推）
    // 复位舵机位置：ID6=500, ID5=408, ID4=129, ID3=177
    // 公式：position = 500 + SERIAL_ANGLE_FACTOR * target_angle
    // 公式：knot_theta = 90 - target_angle (仅肩部)
    // 计算：
    // ID6=500 → target=0° → knot0=0°
    // ID5=408 → target=-22° → knot1=90-(-22)=112°
    // ID4=129 → target=-89° → knot2=-89°
    // ID3=177 → target=-77° → knot3=-77°
    static const float reset_angles[4] = {
        0.0f,    // 关节0：基座0度
        112.0f,  // 关节1：肩部112度（向后倾斜）
        -89.0f,  // 关节2：肘部-89度（接近下限）
        -77.0f   // 关节3：腕部-77度
    };
    
    // LeArm 角度转换（参考 robot_arm.c 中的 theta2servo 函数）
    // 关节0,2,3：直接使用角度
    // 关节1（肩部）：90° - theta（方向相反）
    float converted_angle;
    if (joint_index == 1)
    {
        converted_angle = 90.0f - joint_angle;  // 肩部反向
    }
    else
    {
        converted_angle = joint_angle;  // 其他关节直接使用
    }

    // 同样转换复位角度
    float converted_reset_angle;
    if (joint_index == 1)
    {
        converted_reset_angle = 90.0f - reset_angles[joint_index];
    }
    else
    {
        converted_reset_angle = reset_angles[joint_index];
    }

    // 计算角度偏移量（相对于复位位置）
    angle_offset = converted_angle - converted_reset_angle;

    // 将角度偏移转换为位置偏移
    // 使用 SERIAL_ANGLE_FACTOR = 4.1667 (与LeArm一致)
    float position_offset = angle_offset * SERIAL_ANGLE_FACTOR;
    
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
    
    // 复位位置对应的舵机ID和位置值（与LeArm硬件保持一致）
    // ID映射：6=基座, 5=肩部, 4=肘部, 3=腕部, 2=旋转, 1=夹爪
    uint8_t servo_ids[6] = {
        SERVO_ID_BASE,      // 6 - 基座旋转（关节0）
        SERVO_ID_SHOULDER,  // 5 - 肩部俯仰（关节1）
        SERVO_ID_ELBOW,     // 4 - 肘部俯仰（关节2）
        SERVO_ID_WRIST,     // 3 - 腕部俯仰（关节3）
        SERVO_ID_ROTATION,  // 2 - 腕部旋转
        SERVO_ID_GRIPPER    // 1 - 夹爪
    };

    uint16_t reset_positions[6] = {
        SERIAL_SERVO6_RESET_DUTY,  // ID 6 基座复位值
        SERIAL_SERVO5_RESET_DUTY,  // ID 5 肩部复位值
        SERIAL_SERVO4_RESET_DUTY,  // ID 4 肘部复位值
        SERIAL_SERVO3_RESET_DUTY,  // ID 3 腕部复位值
        SERIAL_SERVO2_RESET_DUTY,  // ID 2 旋转复位值
        SERIAL_SERVO1_RESET_DUTY   // ID 1 夹爪复位值
    };

    // 控制所有6个舵机复位
    for (uint8_t i = 0; i < 6; i++)
    {
        if (i > 0)
        {
            R_BSP_SoftwareDelay(SERVO_FRAME_INTERVAL_MS, BSP_DELAY_UNITS_MILLISECONDS);
        }
        Servo_PositionSet(&arm_ctrl->servo_ctrl, servo_ids[i], reset_positions[i], duration);
    }
	
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

void ArmControl_UnloadAll(void)
{
	uint8_t ids[ARM_MAX_SERVOS_NUM] = {
		SERVO_ID_GRIPPER, SERVO_ID_ROTATION, SERVO_ID_WRIST,
		SERVO_ID_ELBOW,   SERVO_ID_SHOULDER, SERVO_ID_BASE
	};
	Servo_MultUnload(ARM_MAX_SERVOS_NUM, ids);
}

bool ArmControl_ReadAllPositions(uint16_t* positions, uint32_t timeout_ms)
{
	if (positions == NULL)
		return false;

	uint8_t ids[ARM_MAX_SERVOS_NUM] = {
		SERVO_ID_GRIPPER, SERVO_ID_ROTATION, SERVO_ID_WRIST,
		SERVO_ID_ELBOW,   SERVO_ID_SHOULDER, SERVO_ID_BASE
	};
	servo_move_param_t pos_data[ARM_MAX_SERVOS_NUM];

	if (!Servo_MultPosRead(ARM_MAX_SERVOS_NUM, ids, pos_data, timeout_ms))
		return false;

	/* 按 servo_id 映射到 positions[id-1] */
	for (uint8_t i = 0; i < ARM_MAX_SERVOS_NUM; i++)
	{
		uint8_t id = pos_data[i].servo_id;
		if (id >= 1 && id <= ARM_MAX_SERVOS_NUM)
			positions[id - 1] = pos_data[i].position;
	}

	return true;
}

/**
 * @brief 舵机位置 → 关节角度（Theta_To_Servo 的逆运算）
 *
 * Theta_To_Servo 中：
 *   target_angle = joint.theta            (joint 0,2,3)
 *   target_angle = 90 - joint.theta       (joint 1, 肩部)
 *   servo_position = 500 + SERIAL_ANGLE_FACTOR × target_angle
 *   servo ID = 6 - joint_index
 *
 * 逆运算：
 *   target_angle = (servo_position - 500) / SERIAL_ANGLE_FACTOR
 *   joint.theta = target_angle            (joint 0,2,3)
 *   joint.theta = 90 - target_angle       (joint 1)
 */
void ArmControl_PositionsToJointAngles(const uint16_t* positions, float* joint_angles)
{
	if (positions == NULL || joint_angles == NULL)
		return;

	/* positions[id-1]，关节 i 对应 servo ID = 6-i → positions[5-i] */
	for (uint8_t i = 0; i < 4; i++)
	{
		float target_angle = ((float)positions[5 - i] - 500.0f) / SERIAL_ANGLE_FACTOR;
		if (i == 1)
			joint_angles[i] = 90.0f - target_angle;
		else
			joint_angles[i] = target_angle;
	}
}

volatile teach_data_t g_teach_data;

void ArmControl_TeachMode(uint32_t read_interval_ms)
{
	kin_obj_t fk_obj;

	ArmControl_UnloadAll();
	R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);

	while (1)
	{
		uint16_t positions[ARM_MAX_SERVOS_NUM] = {0};
		bool ok = ArmControl_ReadAllPositions(positions, 500);

		if (ok)
		{
			float angles[4];
			ArmControl_PositionsToJointAngles(positions, angles);

			for (uint8_t i = 0; i < 4; i++)
			{
				fk_obj.joint[i].theta = angles[i];
				fk_obj.joint[i].rad   = angles[i] * PI / 180.0f;
			}
			Kin_Forward(&fk_obj);

			/* 写入全局观察变量 */
			g_teach_data.x     = fk_obj.vector.x;
			g_teach_data.y     = fk_obj.vector.y;
			g_teach_data.z     = fk_obj.vector.z;
			g_teach_data.pitch = fk_obj.alpha_pitch;
			for (uint8_t i = 0; i < 4; i++)
				g_teach_data.joint_angles[i] = angles[i];
			for (uint8_t i = 0; i < ARM_MAX_SERVOS_NUM; i++)
				g_teach_data.servo_pos[i] = positions[i];
			g_teach_data.valid = true;
		}
		else
		{
			g_teach_data.valid = false;
		}

		R_BSP_SoftwareDelay(read_interval_ms, BSP_DELAY_UNITS_MILLISECONDS);
	}
}

bool g_result1_state, g_result2_state;	//debug
uint8_t ArmControl_CoordinateSet(float target_x, float target_y, float target_z, 
								 float pitch, float min_pitch, float max_pitch,
								 uint16_t time)
{
	bool result1_state, result2_state;
	kin_obj_t kin_obj_result1, kin_obj_result2;
	kin_vec_t vec;
	
	vec.x = target_x;
	vec.y = target_y;
	vec.z = target_z;
	
	result1_state = PitchRange_Set(&kin_obj_result1, &vec, pitch, min_pitch);
	g_result1_state = result1_state;
	result2_state = PitchRange_Set(&kin_obj_result2, &vec, pitch, max_pitch);
	g_result2_state = result2_state;
	
	if(result1_state) 
	{
		g_kin_obj.alpha_pitch = kin_obj_result1.alpha_pitch;
		g_kin_obj.vector.x = kin_obj_result1.vector.x;
		g_kin_obj.vector.y = kin_obj_result1.vector.y;
		g_kin_obj.vector.z = kin_obj_result1.vector.z;
		
		for (uint8_t i = 0; i < 4; i++) 
		{
			g_kin_obj.joint[i].theta = kin_obj_result1.joint[i].theta;
		}
		
		if (result2_state)
		{
			if (fabs(kin_obj_result2.alpha_pitch - pitch) < fabs(kin_obj_result1.alpha_pitch - pitch))
			{
				g_kin_obj.alpha_pitch = kin_obj_result2.alpha_pitch;
				g_kin_obj.vector.x = kin_obj_result2.vector.x;
				g_kin_obj.vector.y = kin_obj_result2.vector.y;
				g_kin_obj.vector.z = kin_obj_result2.vector.z;
				for (uint8_t i = 0; i< 4; i++)
				{
					g_kin_obj.joint[i].theta = kin_obj_result2.joint[i].theta;
				}			
			}
		}
	}
	else
	{
		if (result2_state)
		{
			g_kin_obj.alpha_pitch = kin_obj_result2.alpha_pitch;
			g_kin_obj.vector.x = kin_obj_result2.vector.x;
			g_kin_obj.vector.y = kin_obj_result2.vector.y;
			g_kin_obj.vector.z = kin_obj_result2.vector.z;
			for (uint8_t i = 0; i< 4; i++)
			{
				g_kin_obj.joint[i].theta = kin_obj_result2.joint[i].theta;
			}
		}
		else
		{
			return false;
		}
	}		
	result1_state = 0;
	result2_state = 0;
	
	Theta_To_Servo(&g_kin_obj, time);
	
	return true;
}







