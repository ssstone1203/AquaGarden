#include "kinematics.h"

/**
 * @brief 角度转弧度（内部辅助函数）
 * 
 * @param theta 角度值（度）
 * @return float 弧度值
 */
static float Theta_To_Rad(float theta)
{
	return theta * PI / 180.0f;
}

/**
 * @brief 弧度转角度（内部辅助函数）
 * 
 * @param rad 弧度值
 * @return float 角度值（度）
 */
static float Rad_To_Theta(float rad)
{
	return rad * 180.0f / PI;
}

void Kinematics_Init(kin_obj_t* kin_obj)
{
	// 参数有效性检查
	if (kin_obj == NULL)
	{
		return;
	}
	
	// 清零整个结构体
	memset(kin_obj, 0, sizeof(kin_obj_t));
	
	// 初始化关节角度为0
	for (uint8_t i = 0; i < 4; i++) 
	{
		kin_obj->joint[i].theta = 0.0f;
		kin_obj->joint[i].rad = 0.0f;
	}
	
	// 初始化位置向量（默认位置）
	kin_obj->vector.x = 15.0f;  // 默认X坐标
	kin_obj->vector.y = 0.0f;   // 默认Y坐标
	kin_obj->vector.z = 2.0f;   // 默认Z坐标
	
	// 初始化俯仰角
	kin_obj->alpha_pitch = 0.0f;
}

/**
 * @brief 正运动学解算（Forward Kinematics）
 * 根据4个关节角度计算末端执行器位置
 * 
 * @param joint0_theta 从下至上第1个关节角度（度）- 基座旋转
 * @param joint1_theta 从下至上第2个关节角度（度）- 肩部俯仰
 * @param joint2_theta 从下至上第3个关节角度（度）- 肘部俯仰
 * @param joint3_theta 从下至上第4个关节角度（度）- 腕部俯仰
 * @return kin_vec_t 末端位置向量 (x, y, z)
 */
kin_vec_t Kinematics_ForwardKinematicsCalc(float joint0_theta, float joint1_theta, float joint2_theta, float joint3_theta)
{
	kin_vec_t result;
	float theta0, theta1, theta2, theta3;  // 弧度值
	float x, y, z;
	
	// 角度转弧度
	theta0 = Theta_To_Rad(joint0_theta);
	theta1 = Theta_To_Rad(joint1_theta);
	theta2 = Theta_To_Rad(joint2_theta);
	theta3 = Theta_To_Rad(joint3_theta);
	
	// 正运动学计算（基于几何法）
	// 机械臂结构说明：
	// - 关节0：基座旋转（绕Z轴）
	// - 关节1：肩部俯仰（绕Y轴）
	// - 关节2：肘部俯仰（绕Y轴）
	// - 关节3：腕部俯仰（绕Y轴）
	
	// 获取各连杆长度
	float L1 = LINKAGE_1;
	float L2 = LINKAGE_2;
	float L3 = LINKAGE_3;
	float L4 = LINKAGE_4;
	
	// 基座旋转后的X方向
	float base_x = L1;
	
	// 肩部关节后的位置
	float shoulder_x = base_x + L2 * cosf(theta1);
	float shoulder_z = L1 + L2 * sinf(theta1);
	
	// 肘部关节后的位置
	float elbow_x = shoulder_x + L3 * cosf(theta1 + theta2);
	float elbow_z = shoulder_z + L3 * sinf(theta1 + theta2);
	
	// 腕部关节后的位置（末端）
	float wrist_x = elbow_x + L4 * cosf(theta1 + theta2 + theta3);
	float wrist_z = elbow_z + L4 * sinf(theta1 + theta2 + theta3);
	
	// 考虑基座旋转，计算最终末端位置
	x = wrist_x * cosf(theta0);
	y = wrist_x * sinf(theta0);
	z = wrist_z;
	
	// 赋值结果
	result.x = x;
	result.y = y;
	result.z = z;
	
	return result;
}

/**
 * @brief 逆运动学解算（Inverse Kinematics）
 * 根据末端位置和俯仰角计算4个关节角度
 * 
 * @param kin_obj 运动学对象指针（输入：vector和alpha_pitch，输出：joint[0..3]）
 * @return kin_status_t KIN_STATUS_OK表示有解，KIN_STATUS_INVALID表示无解
 */
kin_status_t Kinematics_InverseKinematicsCalc(kin_obj_t* kin_obj)
{
	// 参数有效性检查
	if (kin_obj == NULL)
	{
		return KIN_STATUS_INVALID;
	}
	
	float x = kin_obj->vector.x;
	float y = kin_obj->vector.y;
	float z = kin_obj->vector.z;
	
	// 获取各连杆长度
	float L1 = LINKAGE_1;
	float L2 = LINKAGE_2;
	float L3 = LINKAGE_3;
	float L4 = LINKAGE_4;
	
	// 计算基座旋转角（关节0）
	float r = sqrtf(x * x + y * y);
	if (r < 0.001f)
	{
		// 如果r太小，基座角度设为0
		kin_obj->joint[0].rad = 0.0f;
		kin_obj->joint[0].theta = 0.0f;
	}
	else
	{
		kin_obj->joint[0].rad = atan2f(y, x);
		kin_obj->joint[0].theta = Rad_To_Theta(kin_obj->joint[0].rad);
	}
	
	// 调整z坐标（减去基座高度）
	float z_adj = z - L1;
	
	// 计算目标点到基座的距离
	float d = sqrtf(r * r + z_adj * z_adj);
	
	// 检查是否在工作空间内
	float max_reach = L2 + L3 + L4;
	float min_reach = fabsf(L2 - L3 - L4);
	
	if (d > max_reach || d < min_reach)
	{
		return KIN_STATUS_INVALID;  // 超出工作空间
	}
	
	// 使用余弦定理计算关节角度
	// 计算关节2的角度
	float cos_theta2 = (L2 * L2 + L3 * L3 - d * d) / (2.0f * L2 * L3);
	
	// 检查是否有解
	if (cos_theta2 > 1.0f || cos_theta2 < -1.0f)
	{
		return KIN_STATUS_INVALID;
	}
	
	// 选择肘部向上或向下的解（这里选择向上）
	float theta2_rad = acosf(cos_theta2);
	kin_obj->joint[2].rad = theta2_rad;
	kin_obj->joint[2].theta = Rad_To_Theta(theta2_rad);
	
	// 计算关节1的角度
	float alpha = atan2f(z_adj, r);
	float beta = acosf((L2 * L2 + d * d - L3 * L3) / (2.0f * L2 * d));
	float theta1_rad = alpha - beta;
	
	kin_obj->joint[1].rad = theta1_rad;
	kin_obj->joint[1].theta = Rad_To_Theta(theta1_rad);
	
	// 计算关节3的角度（腕部角度）
	// 目标俯仰角减去前面关节的角度
	float target_pitch_rad = Theta_To_Rad(kin_obj->alpha_pitch);
	float theta3_rad = target_pitch_rad - theta1_rad - theta2_rad;
	
	kin_obj->joint[3].rad = theta3_rad;
	kin_obj->joint[3].theta = Rad_To_Theta(theta3_rad);
	
	// 检查角度限制（与joint数组索引对应）
	if (kin_obj->joint[0].theta < MIN_JOINT0_ANGLE || kin_obj->joint[0].theta > MAX_JOINT0_ANGLE ||
		kin_obj->joint[1].theta < MIN_JOINT1_ANGLE || kin_obj->joint[1].theta > MAX_JOINT1_ANGLE ||
		kin_obj->joint[2].theta < MIN_JOINT2_ANGLE || kin_obj->joint[2].theta > MAX_JOINT2_ANGLE ||
		kin_obj->joint[3].theta < MIN_JOINT3_ANGLE || kin_obj->joint[3].theta > MAX_JOINT3_ANGLE)
	{
		return KIN_STATUS_INVALID;
	}
	
	return KIN_STATUS_OK;
}


