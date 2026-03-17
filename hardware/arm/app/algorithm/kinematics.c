#include "kinematics.h"

//#define ELBOW_DOWN
#define ELBOW_UP

//#define KIN_FIRST_INVERSE

/**
 * @brief 角度转弧度
 * 
 * @param theta 角度值（度）
 * @return float 弧度值
 */
static float Theta_To_Rad(float theta)
{
	return theta * PI / 180.0f;
}

/**
 * @brief 弧度转角度
 * 
 * @param rad 弧度值
 * @return float 角度值（度）
 */
static float Rad_To_Theta(float rad)
{
	return rad * 180.0f / PI;
}

void Kin_Init(kin_obj_t* kin_obj)
{
	if(kin_obj ==NULL)
	{
		return;
	}
	
	memset(kin_obj, 0, sizeof(kin_obj_t));
	
	// 初始化关节角度为0
	for(uint8_t i = 0; i < 4; i++)
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

//几何法
//顺运动学解算
kin_vec_t Kin_Forward(float joint0_theta, float joint1_theta, float joint2_theta, float joint3_theta)
{
	kin_vec_t kin_vec_result;
	float rad0, rad1, rad2, rad3;
	float end_x, end_y, end_z;
	float L1,L2,L3,L4;
	
	// 角度转弧度
    rad0 = Theta_To_Rad(joint0_theta);
    rad1 = Theta_To_Rad(joint1_theta);
    rad2 = Theta_To_Rad(joint2_theta);
    rad3 = Theta_To_Rad(joint3_theta);
	
	// 正运动学计算（基于几何法）
    // 假设机械臂结构：
    // - 关节0：基座旋转（绕Z轴）
    // - 关节1：肩部俯仰（绕Y轴）
    // - 关节2：肘部俯仰（绕Y轴）
    // - 关节3：腕部俯仰（绕Y轴）
	
	// 计算各关节在XY平面的投影
    L1 = LINKAGE_1;
    L2 = LINKAGE_2;
    L3 = LINKAGE_3;
	L4 = LINKAGE_4;
	
	//从基座坐标系原点到第1个关节（肩关节）的水平距离为L1
	float base_x;
	base_x = L1;
	
	// 肩部关节后的位置
	float shoulder_x, shoulder_z;
	shoulder_x = base_x + L2 * cosf(rad1);
	shoulder_z = L1 + L2 * sinf(rad1);
	
	// 肘部关节后的位置
	float elbow_x,elbow_z;
	elbow_x = shoulder_x + L3 * cosf(rad1 + rad2);
	elbow_z = shoulder_z + L3 * sinf(rad1 + rad2);

	// 腕部关节后的位置（末端）
	float wrist_x,wrist_z;
	wrist_x = elbow_x + L4 * cosf(rad1 + rad2 + rad3);
	wrist_z = elbow_z + L4 * sinf(rad1 + rad2 + rad3);
	
	// 考虑基座旋转
    end_x = wrist_x * cosf(rad0);
    end_y = wrist_x * sinf(rad0);
    end_z = wrist_z;
	
	kin_vec_result.x = end_x;
	kin_vec_result.y = end_y;
	kin_vec_result.z = end_z;
	
	return kin_vec_result;
}

#ifdef KIN_FIRST_INVERSE
//逆运动学解算
kin_status_t Kin_Inverse(kin_obj_t* kin_obj)
{
	if(kin_obj == NULL){
		return KIN_STATUS_INVALID;
	}
	
	float end_x, end_y, end_z;
	end_x = kin_obj->vector.x;
	end_y = kin_obj->vector.y;
	end_z = kin_obj->vector.z;
	
	float L1,L2,L3,L4;
	L1 = LINKAGE_1;
	L2 = LINKAGE_2;
	L3 = LINKAGE_3;
	L4 = LINKAGE_4;
	
	// 计算基座旋转角（关节0）
	float r;
	r = sqrtf(end_x * end_x + end_y * end_y);
	
	// 如果r太小，基座角度设为0
	if(r < 0.001f){
		kin_obj->joint[0].rad = 0.0f;
        kin_obj->joint[0].theta = 0.0f;
	}else{
		kin_obj->joint[0].rad = atan2f(end_y, end_x);
        kin_obj->joint[0].theta = Rad_To_Theta(kin_obj->joint[0].rad);
	}
	
	// 调整z坐标（减去基座高度）
	float z_adj;
    z_adj = end_z - L1;
	
	// 计算腕部位置：腕部 = 目标点 - L4沿俯仰方向
	// 俯仰角alpha_pitch为末端执行器相对水平面的角度（弧度）
	float alpha_rad = Theta_To_Rad(kin_obj->alpha_pitch);
	float wrist_r = r - L4 * cosf(alpha_rad);
	float wrist_z = z_adj - L4 * sinf(alpha_rad);
	
	// 计算肩部到腕部的距离（用于L2-L3三角形的余弦定理）
	float d;
    d = sqrtf(wrist_r * wrist_r + wrist_z * wrist_z);
	
	// 检查是否在工作空间内（L2+L3需能到达腕部）
	float max_reach,min_reach;
    max_reach = L2 + L3;
    min_reach = fabsf(L2 - L3);
	
	if (d > max_reach || d < min_reach) {
        return KIN_STATUS_INVALID;  // 超出工作空间
    }
	
	// 使用余弦定理计算关节角度
    // 计算关节2的角度（肘部角度）
	float cos_theta2;
    cos_theta2 = (L2 * L2 + L3 * L3 - d * d) / (2.0f * L2 * L3);
	
	// 检查是否有解
    if (cos_theta2 > 1.0f || cos_theta2 < -1.0f) {
        return KIN_STATUS_INVALID;
    }
	
	#ifdef ELBOW_UP
	// 选择肘部向上或向下的解（这里选择向上）
	float theta2_rad;
    theta2_rad = acosf(cos_theta2);
    kin_obj->joint[2].rad = theta2_rad;
    kin_obj->joint[2].theta = Rad_To_Theta(theta2_rad);
	
	// 计算关节1的角度（使用腕部方向）
	float alpha,beta,theta1_rad;
    alpha = atan2f(wrist_z, wrist_r);
    beta = acosf((L2 * L2 + d * d - L3 * L3) / (2.0f * L2 * d));
    theta1_rad = alpha - beta;
	
	kin_obj->joint[1].rad = theta1_rad;
    kin_obj->joint[1].theta = Rad_To_Theta(theta1_rad);
	#endif
	
	#ifdef ELBOW_DOWN
		// 选择肘部向下的解
	float theta2_rad;
    theta2_rad = -acosf(cos_theta2);
    kin_obj->joint[2].rad = theta2_rad;
    kin_obj->joint[2].theta = Rad_To_Theta(theta2_rad);
	
	// 计算关节1的角度（使用腕部方向，肘部下时用 alpha + beta）
	float alpha,beta,theta1_rad;
    alpha = atan2f(wrist_z, wrist_r);
    beta = acosf((L2 * L2 + d * d - L3 * L3) / (2.0f * L2 * d));
    theta1_rad = alpha + beta;
	
	kin_obj->joint[1].rad = theta1_rad;
    kin_obj->joint[1].theta = Rad_To_Theta(theta1_rad);
	#endif
	
	// 计算关节3的角度（腕部角度）
    // 目标俯仰角减去前面关节的角度
	float target_pitch_rad,theta3_rad;
	target_pitch_rad = Theta_To_Rad(kin_obj->alpha_pitch);
    theta3_rad = target_pitch_rad - theta1_rad - theta2_rad;
    
    kin_obj->joint[3].rad = theta3_rad;
    kin_obj->joint[3].theta = Rad_To_Theta(theta3_rad);
	
	// 检查角度限制
    if (kin_obj->joint[0].theta < MIN_JOINT0_ANGLE || kin_obj->joint[0].theta > MAX_JOINT0_ANGLE ||
        kin_obj->joint[1].theta < MIN_JOINT1_ANGLE || kin_obj->joint[1].theta > MAX_JOINT1_ANGLE ||
        kin_obj->joint[2].theta < MIN_JOINT2_ANGLE || kin_obj->joint[2].theta > MAX_JOINT2_ANGLE ||
        kin_obj->joint[3].theta < MIN_JOINT3_ANGLE || kin_obj->joint[3].theta > MAX_JOINT3_ANGLE) {
        return KIN_STATUS_INVALID;
    }
    
    return KIN_STATUS_OK;
}
#endif

kin_status_t Kin_Inverse(kin_obj_t* kin_obj)
{
	if(kin_obj == NULL){
		return KIN_STATUS_INVALID;
	}
	
	float end_x, end_y, end_z;
	end_x = kin_obj->vector.x;
	end_y = kin_obj->vector.y;
	end_z = kin_obj->vector.z;
//	alpha = kin_obj->alpha_pitch;
	
	float L1,L2,L3,L4;
	L1 = LINKAGE_1;
	L2 = LINKAGE_2;
	L3 = LINKAGE_3;
	L4 = LINKAGE_4;
	
	// 计算基座旋转角（关节0）
	float len;	//len为俯视机械臂投影在xy平面上的长度
	len = sqrtf(end_x * end_x + end_y * end_y);
	
	// 如果r太小，基座角度设为0
	if(len < 0.001f){
		kin_obj->joint[0].rad = 0.0f;
        kin_obj->joint[0].theta = 0.0f;
	}else{
		kin_obj->joint[0].rad = atan2f(end_y, end_x);
        kin_obj->joint[0].theta = Rad_To_Theta(kin_obj->joint[0].rad);
	}
	
	// 计算目标点到基座的距离(这是3维空间距离)
    float dist;
	dist = sqrtf(len * len + end_z * end_z);
//	dist = sqrtf(len * len + (end_z - L1) * (end_z - L1));  // ✅ 正确
    
    // 检查是否在工作空间内
    float max_reach,min_reach;
	max_reach = L2 + L3 + L4;
	min_reach = fabsf(L2 - L3 - L4);
	
	if (dist > max_reach || dist < min_reach) {
        return KIN_STATUS_INVALID;  // 超出工作空间
    }
	
	// 计算腕部(关节2)角度(kin_obj->joint[2])
	float a, b;
	a = len - L4*cosf(Theta_To_Rad(kin_obj->alpha_pitch));
	b = end_z - L1 - L4*sinf(Theta_To_Rad(kin_obj->alpha_pitch));
	
	float cos_joint2_rad, sin_joint2_rad;
	cos_joint2_rad = ((a*a + b*b - L2*L2 - L3*L3) / (2.0f*L2*L3));	//pdf中的
//	cos_joint2_rad = (L2*L2 + L3*L3 - (a*a + b*b)) / (2.0f*L2*L3);  // ✅;
	sin_joint2_rad = -sqrtf(1 - (cos_joint2_rad*cos_joint2_rad));
	
	kin_obj->joint[2].rad = atan2f(sin_joint2_rad, cos_joint2_rad);
	kin_obj->joint[2].theta = Rad_To_Theta(kin_obj->joint[2].rad);
	
	//计算肩部(关节1)角度(kin_obj->joint[1])
	float c, d;
	c = L2 + L3*cos_joint2_rad;
	d = L3*sin_joint2_rad;
	//kin_obj->joint[1].rad = (atanf(c / d) - atanf(a / b));
	kin_obj->joint[1].rad = atan2f(c, d) - atan2f(a, b);  // ✅ 正确
	kin_obj->joint[1].theta = Rad_To_Theta(kin_obj->joint[1].rad);
	
	//计算腕部(关节3)角度(kin_obj->joint[3])
	kin_obj->joint[3].theta = kin_obj->alpha_pitch - kin_obj->joint[1].theta - kin_obj->joint[2].theta;
	kin_obj->joint[3].rad = Theta_To_Rad(kin_obj->joint[3].theta);
	
	// 检查角度限制
    if (kin_obj->joint[0].theta < MIN_JOINT0_ANGLE || kin_obj->joint[0].theta > MAX_JOINT0_ANGLE ||
        kin_obj->joint[1].theta < MIN_JOINT1_ANGLE || kin_obj->joint[1].theta > MAX_JOINT1_ANGLE ||
        kin_obj->joint[2].theta < MIN_JOINT2_ANGLE || kin_obj->joint[2].theta > MAX_JOINT2_ANGLE ||
        kin_obj->joint[3].theta < MIN_JOINT3_ANGLE || kin_obj->joint[3].theta > MAX_JOINT3_ANGLE) {
        return KIN_STATUS_INVALID;
    }
	
	return KIN_STATUS_OK;
}

/**
 * @brief 设置机械臂pitch可转动的范围
 * 尝试在给定的俯仰角范围内求解逆运动学
 */
kin_status_t g_kin_inverse1, g_kin_inverse2,g_kin_inverse_mid;
bool PitchRange_Set(kin_obj_t* kin_obj,kin_vec_t* kin_vec, float alpha1, float alpha2)
{
	if (kin_obj == NULL || kin_vec == NULL) {
        return false;
    }
	
	// 复制目标位置
    kin_obj->vector.x = kin_vec->x;
    kin_obj->vector.y = kin_vec->y;
    kin_obj->vector.z = kin_vec->z;
	
	// 确保alpha1 < alpha2
	float temp;
    if (alpha1 > alpha2) {
        temp = alpha1;
        alpha1 = alpha2;
        alpha2 = temp;
    }
	
	// 尝试在范围内求解
    // 先尝试alpha1
    kin_obj->alpha_pitch = alpha1;
	
	g_kin_inverse1 = Kin_Inverse(kin_obj);
	if(g_kin_inverse1 == KIN_STATUS_OK){
		return true;
	}
//    if (Kin_Inverse(kin_obj) == KIN_STATUS_OK) {
//        return true;
//    }
	// 再尝试alpha2
    kin_obj->alpha_pitch = alpha2;
	
	g_kin_inverse2 = Kin_Inverse(kin_obj);
	if(g_kin_inverse2 == KIN_STATUS_OK){
		return true;
	}
//    if (Kin_Inverse(kin_obj) == KIN_STATUS_OK) {
//        return true;
//    }
	
	// 如果两个边界都无解，尝试中间值
	float alpha_mid;
    alpha_mid = (alpha1 + alpha2) / 2.0f;
    kin_obj->alpha_pitch = alpha_mid;
	g_kin_inverse_mid = Kin_Inverse(kin_obj);
	if(g_kin_inverse_mid == KIN_STATUS_OK){
		return true;
	}
//    if (Kin_Inverse(kin_obj) == KIN_STATUS_OK) {
//        return true;
//    }
    
    return false;
}