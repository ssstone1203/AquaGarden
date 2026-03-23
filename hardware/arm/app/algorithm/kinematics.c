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

/**
 * @brief 正向运动学：根据 joint[0..3].theta 计算末端 XYZ 和 alpha_pitch
 *
 * 坐标系与 Kin_Inverse / Theta_To_Servo 一致：
 *   joint[0] = 基座旋转 (θ₀)
 *   joint[1] = 肩部俯仰 (θ₁)
 *   joint[2] = 肘部俯仰 (θ₂)
 *   joint[3] = 腕部俯仰 (θ₃)
 */
void Kin_Forward(kin_obj_t* kin_obj)
{
	if (kin_obj == NULL)
		return;

	float t0 = Theta_To_Rad(kin_obj->joint[0].theta);
	float t1 = Theta_To_Rad(kin_obj->joint[1].theta);
	float t12 = t1 + Theta_To_Rad(kin_obj->joint[2].theta);
	float t123 = t12 + Theta_To_Rad(kin_obj->joint[3].theta);

	float len = LINKAGE_2 * cosf(t1) + LINKAGE_3 * cosf(t12) + LINKAGE_4 * cosf(t123);
	float z   = LINKAGE_1 + LINKAGE_2 * sinf(t1) + LINKAGE_3 * sinf(t12) + LINKAGE_4 * sinf(t123);

	kin_obj->vector.x = len * cosf(t0);
	kin_obj->vector.y = len * sinf(t0);
	kin_obj->vector.z = z;
	kin_obj->alpha_pitch = kin_obj->joint[1].theta
	                     + kin_obj->joint[2].theta
	                     + kin_obj->joint[3].theta;
}

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
	
	// 计算腕部目标点（去除末端连杆 L4 的影响）
    float alpha_rad = Theta_To_Rad(kin_obj->alpha_pitch);
	float a, b;
	a = len - L4*cosf(alpha_rad);
	b = end_z - L1 - L4*sinf(alpha_rad);

	// 用腕部距离检查工作空间（正确做法）
	float wrist_dist2 = a*a + b*b;
	float max_wrist   = L2 + L3;
	float min_wrist   = fabsf(L2 - L3);
	if (wrist_dist2 > max_wrist * max_wrist || wrist_dist2 < min_wrist * min_wrist) {
		return KIN_STATUS_INVALID;
	}

	// 计算肘部(关节2)角度
	float cos_joint2_rad, sin_joint2_rad;
	cos_joint2_rad = (a*a + b*b - L2*L2 - L3*L3) / (2.0f*L2*L3);
	// 限幅防止浮点误差导致 acos/sqrt 溢出
	if (cos_joint2_rad >  1.0f) cos_joint2_rad =  1.0f;
	if (cos_joint2_rad < -1.0f) cos_joint2_rad = -1.0f;
	sin_joint2_rad = -sqrtf(1.0f - cos_joint2_rad*cos_joint2_rad);
	
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
 * @brief 在 [alpha1, alpha2] 区间内以 2° 步长扫描，找到第一个有效的 IK 解。
 *        比原来只尝试 3 个点的方案覆盖更全面，避免漏解。
 */
bool PitchRange_Set(kin_obj_t* kin_obj, kin_vec_t* kin_vec, float alpha1, float alpha2)
{
    if (kin_obj == NULL || kin_vec == NULL)
        return false;

    kin_obj->vector.x = kin_vec->x;
    kin_obj->vector.y = kin_vec->y;
    kin_obj->vector.z = kin_vec->z;

    // 保证 lo <= hi
    float lo = alpha1, hi = alpha2;
    if (lo > hi) { float t = lo; lo = hi; hi = t; }

    // 以 2° 为步长从 lo 扫到 hi
    #define PITCH_STEP 2.0f
    float a = lo;
    while (a <= hi + 0.01f)
    {
        kin_obj->alpha_pitch = a;
        if (Kin_Inverse(kin_obj) == KIN_STATUS_OK)
            return true;
        a += PITCH_STEP;
    }

    // 补充检查精确的 hi 边界（步长可能跳过）
    kin_obj->alpha_pitch = hi;
    if (Kin_Inverse(kin_obj) == KIN_STATUS_OK)
        return true;

    return false;
}